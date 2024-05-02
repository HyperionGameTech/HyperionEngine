/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/FinalPass.hpp>
#include <rendering/Shader.hpp>

#include <util/MeshBuilder.hpp>

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

    virtual Result operator()() override
    {
        g_safe_deleter->SafeRelease(std::move(final_pass.m_ui_texture));

        if (final_pass.m_render_texture_to_screen_pass != nullptr) {
            const DescriptorTableRef &descriptor_table = final_pass.m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable();
            AssertThrow(descriptor_table.IsValid());

            for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(HYP_NAME(RenderTextureToScreenDescriptorSet), frame_index);
                AssertThrow(descriptor_set != nullptr);

                descriptor_set->SetElement(HYP_NAME(InTexture), texture->GetImageView());
            }
        }

        // Set frames to be dirty so the descriptor sets get updated before we render the UI
        final_pass.m_dirty_frame_indices = 0xFFu;
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
    const Configuration &config = g_engine->GetConfig();

    ShaderProperties final_output_props;
    final_output_props.Set("TEMPORAL_AA", config.Get(CONFIG_TEMPORAL_AA));

    if (config.Get(CONFIG_DEBUG_SSR)) {
        final_output_props.Set("DEBUG_SSR");
    } else if (config.Get(CONFIG_DEBUG_HBAO)) {
        final_output_props.Set("DEBUG_HBAO");
    } else if (config.Get(CONFIG_DEBUG_HBIL)) {
        final_output_props.Set("DEBUG_HBIL");
    } else if (config.Get(CONFIG_DEBUG_REFLECTIONS)) {
        final_output_props.Set("DEBUG_REFLECTIONS");
    } else if (config.Get(CONFIG_DEBUG_IRRADIANCE)) {
        final_output_props.Set("DEBUG_IRRADIANCE");
    } else if (config.Get(CONFIG_PATHTRACER)) {
        final_output_props.Set("PATHTRACER");
    }

    m_shader = g_shader_manager->GetOrCreate(
        HYP_NAME(Composite),
        final_output_props
    );

    AssertThrow(InitObject(m_shader));
}

void CompositePass::Create()
{
    CreateShader();
    FullScreenPass::CreateQuad();
    FullScreenPass::CreateCommandBuffers();
    FullScreenPass::CreateFramebuffer();

    RenderableAttributeSet renderable_attributes(
        MeshAttributes {
            .vertex_attributes = static_mesh_vertex_attributes
        },
        MaterialAttributes {
            .fill_mode      = FillMode::FILL,
            .blend_function = BlendFunction::Default(),
            .flags          = MaterialAttributes::RAF_NONE
        }
    );

    FullScreenPass::CreatePipeline(renderable_attributes);
}

void CompositePass::Record(uint frame_index)
{
    FullScreenPass::Record(frame_index);
}

void CompositePass::Render(Frame *frame)
{
    uint frame_index = frame->GetFrameIndex();

    GetFramebuffer()->BeginCapture(frame_index, frame->GetCommandBuffer());

    const CommandBufferRef &command_buffer = m_command_buffers[frame_index];
    HYPERION_ASSERT_RESULT(command_buffer->SubmitSecondary(frame->GetCommandBuffer()));

    GetFramebuffer()->EndCapture(frame_index, frame->GetCommandBuffer());
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
    if (m_render_texture_to_screen_pass != nullptr) {
        m_render_texture_to_screen_pass->Destroy();
        m_render_texture_to_screen_pass.Reset();
    }

    // Prevent any dangling pointers from render commands
    HYP_SYNC_RENDER();
}

void FinalPass::SetUITexture(Handle<Texture> texture)
{
    if (!texture.IsValid()) {
        texture = CreateObject<Texture>(Texture2D(
            Extent2D { 1, 1 },
            InternalFormat::RGBA8,
            FilterMode::TEXTURE_FILTER_LINEAR,
            WrapMode::TEXTURE_WRAP_REPEAT,
            nullptr
        ));
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
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    m_extent = g_engine->GetGPUInstance()->GetSwapchain()->extent;
    m_image_format = g_engine->GetGPUInstance()->GetSwapchain()->image_format;

    m_composite_pass.Create();

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
            ->SetElement(HYP_NAME(FinalOutputTexture), m_composite_pass.GetAttachmentUsage(0)->GetImageView());
    }

    FullScreenPass::CreateQuad();

    auto shader = g_shader_manager->GetOrCreate(HYP_NAME(Blit));
    AssertThrow(InitObject(shader));

    uint iteration = 0;

    m_attachments.PushBack(MakeRenderObject<renderer::Attachment>(
        MakeRenderObject<Image>(renderer::FramebufferImage2D(
            m_extent,
            m_image_format,
            nullptr
        )),
        renderer::RenderPassStage::PRESENT
    ));

    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(g_engine->GetGPUDevice()));
    }

    for (const ImageRef &image_ref : g_engine->GetGPUInstance()->GetSwapchain()->GetImages()) {
        auto fbo = CreateObject<Framebuffer>(
            m_extent,
            renderer::RenderPassStage::PRESENT,
            renderer::RenderPassMode::RENDER_PASS_INLINE
        );

        ImageViewRef color_attachment_image_view = MakeRenderObject<renderer::ImageView>();
        HYPERION_ASSERT_RESULT(color_attachment_image_view->Create(
            g_engine->GetGPUDevice(),
            image_ref.Get()
        ));

        SamplerRef color_attachment_sampler_ref = g_engine->GetPlaceholderData()->GetSamplerLinear();

        AttachmentUsageRef color_attachment_usage = MakeRenderObject<renderer::AttachmentUsage>(
            m_attachments[0],
            std::move(color_attachment_image_view),
            std::move(color_attachment_sampler_ref),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE
        );

        color_attachment_usage->SetBinding(0);
        HYPERION_ASSERT_RESULT(color_attachment_usage->Create(g_engine->GetGPUDevice()));
        fbo->AddAttachmentUsage(color_attachment_usage);

        if (iteration == 0) {
            m_render_group = CreateObject<RenderGroup>(
                std::move(shader),
                RenderableAttributeSet(
                    MeshAttributes {
                        .vertex_attributes = static_mesh_vertex_attributes
                    },
                    MaterialAttributes {
                        .bucket = BUCKET_SWAPCHAIN
                    }
                )
            );
        }

        m_render_group->AddFramebuffer(std::move(fbo));

        ++iteration;
    }

    InitObject(m_render_group);

    // Create final image to hold last frame's color texture

    m_last_frame_image = MakeRenderObject<renderer::Image>(renderer::TextureImage(
        Extent3D { m_extent.width, m_extent.height, 1 },
        InternalFormat::RGBA8_SRGB,
        ImageType::TEXTURE_TYPE_2D,
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        nullptr
    ));

    HYPERION_ASSERT_RESULT(m_last_frame_image->Create(g_engine->GetGPUDevice()));

    // Create UI stuff
    InitObject(m_ui_texture);

    Handle<Shader> render_texture_to_screen_shader = g_shader_manager->GetOrCreate(HYP_NAME(RenderTextureToScreen));
    AssertThrow(InitObject(render_texture_to_screen_shader));

    const renderer::DescriptorTableDeclaration descriptor_table_decl = render_texture_to_screen_shader->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef descriptor_table = MakeRenderObject<renderer::DescriptorTable>(descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(HYP_NAME(RenderTextureToScreenDescriptorSet), frame_index);
        AssertThrow(descriptor_set != nullptr);

        if (m_ui_texture.IsValid()) {
            descriptor_set->SetElement(HYP_NAME(InTexture), m_ui_texture->GetImageView());
        } else {
            descriptor_set->SetElement(HYP_NAME(InTexture), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }
    }

    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    m_render_texture_to_screen_pass.Reset(new FullScreenPass(
        render_texture_to_screen_shader,
        std::move(descriptor_table),
        m_image_format,
        m_extent
    ));

    m_render_texture_to_screen_pass->SetBlendFunction(BlendFunction(
        BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
        BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA
    ));

    m_render_texture_to_screen_pass->Create();
}

void FinalPass::Destroy()
{
    m_composite_pass.Destroy();

    SafeRelease(std::move(m_last_frame_image));

    FullScreenPass::Destroy();
}

void FinalPass::Record(uint frame_index)
{
}

void FinalPass::Render(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const uint frame_index = frame->GetFrameIndex();

    const GraphicsPipelineRef &pipeline = m_render_group->GetPipeline();
    const uint acquired_image_index = g_engine->GetGPUInstance()->GetFrameHandler()->GetAcquiredImageIndex();

    m_composite_pass.Record(frame->GetFrameIndex());
    m_composite_pass.Render(frame);

    { // Copy result to store previous frame's color buffer
        const ImageRef &source_image = m_composite_pass.GetAttachments()[0]->GetImage();

        source_image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_SRC);
        m_last_frame_image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_DST);

        m_last_frame_image->Blit(frame->GetCommandBuffer(), source_image);

        source_image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    }

    m_render_group->GetFramebuffers()[acquired_image_index]->BeginCapture(0, frame->GetCommandBuffer());

    pipeline->Bind(frame->GetCommandBuffer());

    pipeline->GetDescriptorTable()->Bind(
        frame,
        pipeline,
        { }
    );

    /* Render full screen quad overlay to blit deferred + all post fx onto screen. */
    m_full_screen_quad->Render(frame->GetCommandBuffer());

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
                    HYP_NAME(Scene),
                    {
                        { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, 0) },
                        { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, 0) },
                        { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                        { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0) },
                        { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                    }
                }
            }
        );

        m_full_screen_quad->Render(frame->GetCommandBuffer());
    }
#endif

    m_render_group->GetFramebuffers()[acquired_image_index]->EndCapture(0, frame->GetCommandBuffer());
}

#pragma endregion FinalPass

} // namespace hyperion
