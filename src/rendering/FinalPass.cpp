/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/FinalPass.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/PlaceholderData.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <scene/Mesh.hpp>

#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <Engine.hpp>

#define HYP_RENDER_UI_IN_COMPOSITE_PASS

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(SetUITexture) : renderer::RenderCommand
{
    FinalPass           &final_pass;
    Handle<Texture>     texture;

    RENDER_COMMAND(SetUITexture)(
        FinalPass &final_pass,
        Handle<Texture> texture
    ) : final_pass(final_pass),
        texture(std::move(texture))
    {
        AssertThrow(this->texture.IsValid());
        AssertThrow(this->texture->GetImageView().IsValid());
    }

    virtual ~RENDER_COMMAND(SetUITexture)() override
    {
        g_safe_deleter->SafeRelease(std::move(texture));
    }

    virtual RendererResult operator()() override
    {
        g_safe_deleter->SafeRelease(std::move(final_pass.m_ui_texture));

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

                descriptor_set->SetElement(NAME("InTexture"), texture->GetImageView());
            }
        }

        // Set frames to be dirty so the descriptor sets get updated before we render the UI
        final_pass.m_dirty_frame_indices = (1u << max_frames_in_flight) - 1;
        final_pass.m_ui_texture = std::move(texture);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region CompositePass

CompositePass::CompositePass()
    : FullScreenPass(InternalFormat::RGBA8_SRGB)
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
    FullScreenPass::CreateCommandBuffers();
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

void CompositePass::Record(uint32 frame_index)
{
    FullScreenPass::Record(frame_index);
}

void CompositePass::Render(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);
    
    uint32 frame_index = frame->GetFrameIndex();

    GetFramebuffer()->BeginCapture(frame->GetCommandBuffer(), frame_index);

    const CommandBufferRef &command_buffer = m_command_buffers[frame_index];
    HYPERION_ASSERT_RESULT(command_buffer->SubmitSecondary(frame->GetCommandBuffer()));

    GetFramebuffer()->EndCapture(frame->GetCommandBuffer(), frame_index);
}

#pragma endregion CompositePass

#pragma region FinalPass

FinalPass::FinalPass()
    : FullScreenPass(),
      m_dirty_frame_indices(0)
{
}

FinalPass::~FinalPass()
{
    SafeRelease(std::move(m_last_frame_image));

    if (m_render_texture_to_screen_pass != nullptr) {
        m_render_texture_to_screen_pass.Reset();
    }

    // @NOTE: FullScreenPass will flush render queue, preventing dangling references from render commands
}

void FinalPass::SetUITexture(Handle<Texture> texture)
{
    if (!texture.IsValid()) {
        texture = CreateObject<Texture>(TextureDesc {
            ImageType::TEXTURE_TYPE_2D,
            InternalFormat::RGBA8,
            Vec3u { 1, 1, 1 },
            FilterMode::TEXTURE_FILTER_LINEAR,
            FilterMode::TEXTURE_FILTER_LINEAR,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
        });
    }

    InitObject(texture);

    PUSH_RENDER_COMMAND(
        SetUITexture,
        *this,
        std::move(texture)
    );
}

void FinalPass::Create()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    m_extent = g_engine->GetGPUInstance()->GetSwapchain()->extent;
    m_image_format = g_engine->GetGPUInstance()->GetSwapchain()->image_format;

    m_composite_pass.Create();

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("FinalOutputTexture"), m_composite_pass.GetAttachment(0)->GetImageView());
    }

    FullScreenPass::CreateQuad();

    ShaderRef shader = g_shader_manager->GetOrCreate(NAME("Blit"));
    AssertThrow(shader.IsValid());

    uint32 iteration = 0;

    for (const ImageRef &image_ref : g_engine->GetGPUInstance()->GetSwapchain()->GetImages()) {
        FramebufferRef fbo = MakeRenderObject<Framebuffer>(
            m_extent,
            renderer::RenderPassStage::PRESENT,
            renderer::RenderPassMode::RENDER_PASS_INLINE
        );
        
        AttachmentRef color_attachment = MakeRenderObject<Attachment>(
            image_ref,
            renderer::RenderPassStage::PRESENT,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE
        );

        color_attachment->SetBinding(0);
        HYPERION_ASSERT_RESULT(color_attachment->Create(g_engine->GetGPUDevice()));

        fbo->AddAttachment(color_attachment);

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

        m_render_group->AddFramebuffer(std::move(fbo));

        ++iteration;
    }

    InitObject(m_render_group);

    // Create final image to hold last frame's color texture

    m_last_frame_image = MakeRenderObject<Image>(renderer::SampledImage(
        Vec3u { m_extent.x, m_extent.y, 1 },
        InternalFormat::RGBA8_SRGB,
        ImageType::TEXTURE_TYPE_2D,
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST
    ));

    HYPERION_ASSERT_RESULT(m_last_frame_image->Create(g_engine->GetGPUDevice()));

    // Create UI stuff
    InitObject(m_ui_texture);

    ShaderRef render_texture_to_screen_shader = g_shader_manager->GetOrCreate(NAME("RenderTextureToScreen_UI"));
    AssertThrow(render_texture_to_screen_shader.IsValid());

    const renderer::DescriptorTableDeclaration descriptor_table_decl = render_texture_to_screen_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        if (m_ui_texture.IsValid()) {
            descriptor_set->SetElement(NAME("InTexture"), m_ui_texture->GetImageView());
        } else {
            descriptor_set->SetElement(NAME("InTexture"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }
    }

    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

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

void FinalPass::Resize_Internal(Vec2u new_size)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    FullScreenPass::Resize_Internal(new_size);

    if (m_render_texture_to_screen_pass != nullptr) {
        m_render_texture_to_screen_pass->Resize(new_size);
    }
}

void FinalPass::Record(uint32 frame_index)
{
}

void FinalPass::Render(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    const GraphicsPipelineRef &pipeline = m_render_group->GetPipeline();
    const uint32 acquired_image_index = g_engine->GetGPUInstance()->GetFrameHandler()->GetAcquiredImageIndex();

    m_composite_pass.Record(frame->GetFrameIndex());
    m_composite_pass.Render(frame);

    { // Copy result to store previous frame's color buffer
        const ImageRef &source_image = m_composite_pass.GetAttachment(0)->GetImage();

        source_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_SRC);
        m_last_frame_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_DST);

        m_last_frame_image->Blit(frame->GetCommandBuffer(), source_image);

        m_last_frame_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
        source_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    }

    m_render_group->GetFramebuffers()[acquired_image_index]->BeginCapture(frame->GetCommandBuffer(), 0);

    pipeline->Bind(frame->GetCommandBuffer());

    pipeline->GetDescriptorTable()->Bind(
        frame,
        pipeline,
        { }
    );

    /* Render full screen quad overlay to blit deferred + all post fx onto screen. */
    m_full_screen_quad->GetRenderResource().Render(frame->GetCommandBuffer());

#ifdef HYP_RENDER_UI_IN_COMPOSITE_PASS
    // Render UI onto screen, blending with the scene render pass
    if (m_ui_texture.IsValid()) {
        // If the UI pass has needs to be updated for the current frame index, do it
        if (m_dirty_frame_indices & (1u << frame_index)) {
            HYPERION_ASSERT_RESULT(
                m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()
                    ->Update(g_engine->GetGPUDevice(), frame_index)
            );

            m_dirty_frame_indices &= ~(1u << frame_index);
        }

        // render previous frame's result to screen
        m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->Bind(frame->GetCommandBuffer());
        m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()->Bind<GraphicsPipelineRef>(
            frame->GetCommandBuffer(),
            frame_index,
            m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline(),
            {
                {
                    NAME("Scene"),
                    { }
                }
            }
        );

        m_full_screen_quad->GetRenderResource().Render(frame->GetCommandBuffer());
    }
#endif

    m_render_group->GetFramebuffers()[acquired_image_index]->EndCapture(frame->GetCommandBuffer(), 0);
}

#pragma endregion FinalPass

} // namespace hyperion
