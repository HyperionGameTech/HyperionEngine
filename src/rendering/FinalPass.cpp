#include <rendering/FinalPass.hpp>
#include <rendering/Shader.hpp>

#include <util/MeshBuilder.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

CompositePass::CompositePass()
    : FullScreenPass(InternalFormat::R11G11B10F)
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

    final_output_props.Set("OUTPUT_SRGB", renderer::IsSRGBFormat(g_engine->GetGPUInstance()->GetSwapchain()->image_format));

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
            .blend_function = BlendFunction::AlphaBlending()
        }
    );

    FullScreenPass::CreatePipeline(renderable_attributes);
}


FinalPass::FinalPass() = default;
FinalPass::~FinalPass() = default;

void FinalPass::Create()
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_composite_pass.Create();

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
            ->SetElement(HYP_NAME(FinalOutputTexture), m_composite_pass.GetAttachmentUsage(0)->GetImageView());
    }

    m_quad = MeshBuilder::Quad();
    AssertThrow(InitObject(m_quad));

    auto shader = g_shader_manager->GetOrCreate(HYP_NAME(Blit));
    AssertThrow(InitObject(shader));

    uint iteration = 0;

    m_attachments.PushBack(MakeRenderObject<renderer::Attachment>(
        MakeRenderObject<Image>(renderer::FramebufferImage2D(
            g_engine->GetGPUInstance()->GetSwapchain()->extent,
            g_engine->GetGPUInstance()->GetSwapchain()->image_format,
            nullptr
        )),
        renderer::RenderPassStage::PRESENT
    ));

    m_attachments.PushBack(MakeRenderObject<renderer::Attachment>(
        MakeRenderObject<Image>(renderer::FramebufferImage2D(
            g_engine->GetGPUInstance()->GetSwapchain()->extent,
            g_engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        )),
        renderer::RenderPassStage::PRESENT
    ));

    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(g_engine->GetGPUDevice()));
    }

    for (const ImageRef &image_ref : g_engine->GetGPUInstance()->GetSwapchain()->GetImages()) {
        auto fbo = CreateObject<Framebuffer>(
            g_engine->GetGPUInstance()->GetSwapchain()->extent,
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

        AttachmentUsageRef depth_attachment_usage = MakeRenderObject<renderer::AttachmentUsage>(
            m_attachments[1],
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE
        );

        depth_attachment_usage->SetBinding(1);
        HYPERION_ASSERT_RESULT(depth_attachment_usage->Create(g_engine->GetGPUDevice()));
        fbo->AddAttachmentUsage(depth_attachment_usage);

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

    m_last_frame_image = MakeRenderObject<renderer::Image>(renderer::TextureImage(
        Extent3D { g_engine->GetGPUInstance()->GetSwapchain()->extent, 1 },
        InternalFormat::R10G10B10A2,
        ImageType::TEXTURE_TYPE_2D,
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        nullptr
    ));

    HYPERION_ASSERT_RESULT(m_last_frame_image->Create(g_engine->GetGPUDevice()));
}

void FinalPass::Destroy()
{
    m_composite_pass.Destroy();

    SafeRelease(std::move(m_attachments));
    SafeRelease(std::move(m_last_frame_image));

    m_render_group.Reset();
    m_quad.Reset();
}

void FinalPass::Render(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const GraphicsPipelineRef &pipeline = m_render_group->GetPipeline();
    const uint acquired_image_index = g_engine->GetGPUInstance()->GetFrameHandler()->GetAcquiredImageIndex();

    { // Copy result to store previous frame's color buffer
        const ImageRef &source_image = m_composite_pass.GetAttachments()[0]->GetImage();

        source_image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_SRC);
        m_last_frame_image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_DST);

        m_last_frame_image->Blit(frame->GetCommandBuffer(), source_image);

        source_image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    }

    m_composite_pass.Record(frame->GetFrameIndex());
    m_composite_pass.Render(frame);


    m_render_group->GetFramebuffers()[acquired_image_index]->BeginCapture(0, frame->GetCommandBuffer());

    pipeline->Bind(frame->GetCommandBuffer());

    pipeline->GetDescriptorTable()->Bind(
        frame,
        pipeline,
        { }
    );

    /* Render full screen quad overlay to blit deferred + all post fx onto screen. */
    m_quad->Render(frame->GetCommandBuffer());

    m_render_group->GetFramebuffers()[acquired_image_index]->EndCapture(0, frame->GetCommandBuffer());
}

} // namespace hyperion::v2
