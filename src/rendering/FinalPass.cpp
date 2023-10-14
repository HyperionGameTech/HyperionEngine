#include <rendering/FinalPass.hpp>
#include <rendering/Shader.hpp>

#include <util/MeshBuilder.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

FinalPass::FinalPass() = default;
FinalPass::~FinalPass() = default;

void FinalPass::Create()
{
    m_quad = MeshBuilder::Quad();
    AssertThrow(InitObject(m_quad));

    const auto &config = g_engine->GetConfig();

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

    final_output_props.Set("OUTPUT_SRGB", renderer::IsSRGBFormat(g_engine->GetGPUInstance()->swapchain->image_format));

    auto shader = g_shader_manager->GetOrCreate(
        HYP_NAME(FinalOutput),
        final_output_props
    );

    AssertThrow(InitObject(shader));

    UInt iteration = 0;

    m_attachments.PushBack(RenderObjects::Make<renderer::Attachment>(
        RenderObjects::Make<Image>(renderer::FramebufferImage2D(
            g_engine->GetGPUInstance()->swapchain->extent,
            g_engine->GetGPUInstance()->swapchain->image_format,
            nullptr
        )),
        renderer::RenderPassStage::PRESENT
    ));

    m_attachments.PushBack(RenderObjects::Make<renderer::Attachment>(
        RenderObjects::Make<Image>(renderer::FramebufferImage2D(
            g_engine->GetGPUInstance()->swapchain->extent,
            g_engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        )),
        renderer::RenderPassStage::PRESENT
    ));
    
    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(g_engine->GetGPUDevice()));
    }

    for (renderer::PlatformImage img : g_engine->GetGPUInstance()->swapchain->images) {
        auto fbo = CreateObject<Framebuffer>(
            g_engine->GetGPUInstance()->swapchain->extent,
            renderer::RenderPassStage::PRESENT,
            renderer::RenderPass::Mode::RENDER_PASS_INLINE
        );

        renderer::AttachmentUsage *color_attachment_usage,
            *depth_attachment_usage;

        HYPERION_ASSERT_RESULT(m_attachments[0]->AddAttachmentUsage(
            g_engine->GetGPUDevice(),
            img,
            renderer::helpers::ToVkFormat(g_engine->GetGPUInstance()->swapchain->image_format),
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D,
            1, 1,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &color_attachment_usage
        ));

        color_attachment_usage->SetBinding(0);

        fbo->AddAttachmentUsage(color_attachment_usage);

        HYPERION_ASSERT_RESULT(m_attachments[1]->AddAttachmentUsage(
            g_engine->GetGPUDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &depth_attachment_usage
        ));

        fbo->AddAttachmentUsage(depth_attachment_usage);

        depth_attachment_usage->SetBinding(1);

        if (iteration == 0) {
            m_render_group = CreateObject<RenderGroup>(
                std::move(shader),
                RenderableAttributeSet(
                    MeshAttributes {
                        .vertex_attributes = renderer::static_mesh_vertex_attributes
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
}

void FinalPass::Destroy()
{
    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Destroy(g_engine->GetGPUDevice()));
    }

    m_attachments.Clear();

    m_render_group.Reset();
    m_quad.Reset();
}

void FinalPass::Render(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const GraphicsPipelineRef &pipeline = m_render_group->GetPipeline();
    const UInt acquired_image_index = g_engine->GetGPUInstance()->GetFrameHandler()->GetAcquiredImageIndex();

    m_render_group->GetFramebuffers()[acquired_image_index]->BeginCapture(0, frame->GetCommandBuffer());
    
    pipeline->Bind(frame->GetCommandBuffer());

    g_engine->GetGPUInstance()->GetDescriptorPool().Bind(
        g_engine->GetGPUDevice(),
        frame->GetCommandBuffer(),
        pipeline,
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

#if HYP_FEATURES_ENABLE_RAYTRACING && HYP_FEATURES_BINDLESS_TEXTURES
    /* TMP */
    g_engine->GetGPUInstance()->GetDescriptorPool().Bind(
        g_engine->GetGPUDevice(),
        frame->GetCommandBuffer(),
        pipeline,
        {{
            .set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
            .count = 1
        }}
    );
#endif

    /* Render full screen quad overlay to blit deferred + all post fx onto screen. */
    m_quad->Render(frame->GetCommandBuffer());
    
    m_render_group->GetFramebuffers()[acquired_image_index]->EndCapture(0, frame->GetCommandBuffer());
}

} // namespace hyperion::v2