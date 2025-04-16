/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/FinalPass.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderTexture.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

#include <scene/Mesh.hpp>
#include <scene/Texture.hpp>

#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <Engine.hpp>

#define HYP_RENDER_UI_IN_COMPOSITE_PASS

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region Render commands

struct RENDER_COMMAND(SetUILayerImageView) : renderer::RenderCommand
{
    FinalPass       &final_pass;
    ImageViewRef    image_view;

    RENDER_COMMAND(SetUILayerImageView)(
        FinalPass &final_pass,
        const ImageViewRef &image_view
    ) : final_pass(final_pass),
        image_view(image_view)
    {
    }

    virtual ~RENDER_COMMAND(SetUILayerImageView)() override = default;

    virtual RendererResult operator()() override
    {
        SafeRelease(std::move(final_pass.m_ui_layer_image_view));

        if (g_engine->IsShuttingDown()) {
            // Don't set if the engine is in a shutdown state,
            // pipeline may already have been deleted.
            HYPERION_RETURN_OK;
        }

        if (final_pass.m_render_texture_to_screen_pass != nullptr) {
            const DescriptorTableRef &descriptor_table = final_pass.m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable();
            AssertThrow(descriptor_table.IsValid());

            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frame_index);
                AssertThrow(descriptor_set != nullptr);

                if (image_view != nullptr) {
                    descriptor_set->SetElement(NAME("InTexture"), image_view);
                } else {
                    descriptor_set->SetElement(NAME("InTexture"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
                }
            }
        }

        // Set frames to be dirty so the descriptor sets get updated before we render the UI
        final_pass.m_dirty_frame_indices = (1u << max_frames_in_flight) - 1;
        final_pass.m_ui_layer_image_view = image_view;

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region CompositePass

CompositePass::CompositePass(Vec2u extent)
    : FullScreenPass(InternalFormat::RGBA16F, extent)
{
}

CompositePass::~CompositePass() = default;

void CompositePass::CreateShader()
{
    const config::ConfigurationTable &config = g_engine->GetAppContext()->GetConfiguration();

    ShaderProperties final_output_props;
    final_output_props.Set("TEMPORAL_AA", config["rendering.taa.enabled"].ToBool());

    if (config["rendering.debug.ssr"]) {
        final_output_props.Set("DEBUG_SSR");
    } else if (config["rendering.debug.hbao"]) {
        final_output_props.Set("DEBUG_HBAO");
    } else if (config["rendering.debug.hbil"]) {
        final_output_props.Set("DEBUG_HBIL");
    } else if (config["rendering.debug.reflections"]) {
        final_output_props.Set("DEBUG_REFLECTIONS");
    } else if (config["rendering.debug.irradiance"]) {
        final_output_props.Set("DEBUG_IRRADIANCE");
    } else if (config["rendering.debug.path_tracer"]) {
        final_output_props.Set("PATHTRACER");
    }

    m_shader = g_shader_manager->GetOrCreate(NAME("Composite"), final_output_props);
}

void CompositePass::Create()
{
    CreateShader();
    FullScreenPass::CreateQuad();
    FullScreenPass::CreateFramebuffer();
    FullScreenPass::CreatePipeline(RenderableAttributeSet(
        MeshAttributes {
            .vertex_attributes = static_mesh_vertex_attributes
        },
        MaterialAttributes {
            .fill_mode      = FillMode::FILL,
            .blend_function = BlendFunction::Default(),
            .flags          = MaterialAttributeFlags::NONE
        }
    ));
}

#pragma endregion CompositePass

#pragma region FinalPass

FinalPass::FinalPass(SwapchainBase *swapchain)
    : FullScreenPass(InternalFormat::RGBA8),
      m_swapchain(swapchain),
      m_dirty_frame_indices(0)
{
}

FinalPass::~FinalPass()
{
    SafeRelease(std::move(m_last_frame_image));

    if (m_render_texture_to_screen_pass != nullptr) {
        m_render_texture_to_screen_pass.Reset();
    }
}

void FinalPass::SetUILayerImageView(const ImageViewRef &image_view)
{
    PUSH_RENDER_COMMAND(SetUILayerImageView, *this, image_view);
}

void FinalPass::Create()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_swapchain != nullptr);

    m_extent = m_swapchain->GetExtent();
    m_image_format = m_swapchain->GetImageFormat();

    m_composite_pass = MakeUnique<CompositePass>(m_extent);
    m_composite_pass->Create();

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("FinalOutputTexture"), m_composite_pass->GetAttachment(0)->GetImageView());
    }

    FullScreenPass::CreateQuad();

    ShaderRef shader = g_shader_manager->GetOrCreate(NAME("Blit"));
    AssertThrow(shader.IsValid());

    uint32 iteration = 0;

    for (const ImageRef &image_ref : m_swapchain->GetImages()) {
        FramebufferRef framebuffer = g_rendering_api->MakeFramebuffer(m_extent, renderer::RenderPassStage::PRESENT);
        
        AttachmentRef color_attachment = framebuffer->AddAttachment(
            0,
            image_ref,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE
        );

        HYPERION_ASSERT_RESULT(color_attachment->Create());

        if (iteration == 0) {
            m_render_group = CreateObject<RenderGroup>(
                shader,
                RenderableAttributeSet(
                    MeshAttributes {
                        .vertex_attributes = static_mesh_vertex_attributes
                    },
                    MaterialAttributes {
                        .bucket = BUCKET_SWAPCHAIN
                    }
                ),
                RenderGroupFlags::NONE
            );
        }

        m_render_group->AddFramebuffer(std::move(framebuffer));

        ++iteration;
    };

    InitObject(m_render_group);

    // Create final image to hold last frame's color texture
    m_last_frame_image = g_rendering_api->MakeImage(TextureDesc {
        renderer::ImageType::TEXTURE_TYPE_2D,
        m_composite_pass->GetFormat(),
        Vec3u { m_extent.x, m_extent.y, 1 },
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::FilterMode::TEXTURE_FILTER_NEAREST,
        renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        1,
        ImageFormatCapabilities::SAMPLED
    });

    HYPERION_ASSERT_RESULT(m_last_frame_image->Create());

    ShaderRef render_texture_to_screen_shader = g_shader_manager->GetOrCreate(NAME("RenderTextureToScreen_UI"));
    AssertThrow(render_texture_to_screen_shader.IsValid());

    const renderer::DescriptorTableDeclaration descriptor_table_decl = render_texture_to_screen_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        if (m_ui_layer_image_view != nullptr) {
            descriptor_set->SetElement(NAME("InTexture"), m_ui_layer_image_view);
        } else {
            descriptor_set->SetElement(NAME("InTexture"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }
    }

    DeferCreate(descriptor_table);

    m_render_texture_to_screen_pass = MakeUnique<FullScreenPass>(
        render_texture_to_screen_shader,
        std::move(descriptor_table),
        m_image_format,
        m_extent
    );

    m_render_texture_to_screen_pass->SetBlendFunction(BlendFunction(
        BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
        BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA
    ));

    m_render_texture_to_screen_pass->Create();
}

void FinalPass::Resize_Internal(Vec2u)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    // Always keep FinalPass the same size as the swapchain
    const Vec2u new_size = m_swapchain->GetExtent();

    FullScreenPass::Resize_Internal(new_size);

    m_composite_pass->Resize(new_size);

    if (m_render_texture_to_screen_pass != nullptr) {
        m_render_texture_to_screen_pass->Resize(new_size);
    }
}

void FinalPass::Render(FrameBase *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    const GraphicsPipelineRef &pipeline = m_render_group->GetPipeline();

    const uint32 acquired_image_index = m_swapchain->GetAcquiredImageIndex();

    m_composite_pass->Render(frame);

    { // Copy result to store previous frame's color buffer
        const ImageRef &source_image = m_composite_pass->GetAttachment(0)->GetImage();

        frame->GetCommandList().Add<InsertBarrier>(source_image, renderer::ResourceState::COPY_SRC);
        frame->GetCommandList().Add<InsertBarrier>(m_last_frame_image, renderer::ResourceState::COPY_DST);

        frame->GetCommandList().Add<Blit>(source_image, m_last_frame_image);

        // put the images back into a state for reading
        frame->GetCommandList().Add<InsertBarrier>(source_image, renderer::ResourceState::SHADER_RESOURCE);
        frame->GetCommandList().Add<InsertBarrier>(m_last_frame_image, renderer::ResourceState::SHADER_RESOURCE);
    }

    frame->GetCommandList().Add<BeginFramebuffer>(m_render_group->GetFramebuffers()[acquired_image_index], 0);

    frame->GetCommandList().Add<BindGraphicsPipeline>(pipeline);

    frame->GetCommandList().Add<BindDescriptorTable>(
        pipeline->GetDescriptorTable(),
        pipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> { },
        frame_index
    );

    /* Render full screen quad overlay to blit deferred + all post fx onto screen. */
    m_full_screen_quad->GetRenderResource().Render(frame->GetCommandList());

#ifdef HYP_RENDER_UI_IN_COMPOSITE_PASS
    // Render UI onto screen, blending with the scene render pass
    if (m_ui_layer_image_view != nullptr) {
        // If the UI pass has needs to be updated for the current frame index, do it
        if (m_dirty_frame_indices & (1u << frame_index)) {
            m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()
                ->Update(frame_index);

            m_dirty_frame_indices &= ~(1u << frame_index);
        }

        // render previous frame's result to screen
        frame->GetCommandList().Add<BindGraphicsPipeline>(m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline());

        frame->GetCommandList().Add<BindDescriptorTable>(
            m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable(),
            m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline(),
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                {
                    NAME("Scene"),
                    { }
                }
            },
            frame_index
        );

        m_full_screen_quad->GetRenderResource().Render(frame->GetCommandList());
    }
#endif

    frame->GetCommandList().Add<EndFramebuffer>(m_render_group->GetFramebuffers()[acquired_image_index], 0);
}

#pragma endregion FinalPass

} // namespace hyperion
