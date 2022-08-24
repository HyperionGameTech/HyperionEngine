#include <rendering/FinalPass.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderPass.hpp>
#include <Engine.hpp>
#include <builders/MeshBuilder.hpp>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Attachment;
using renderer::ImageView;
using renderer::FramebufferObject;

FinalPass::FinalPass()
{
}

FinalPass::~FinalPass()
{
}

void FinalPass::Create(Engine *engine)
{
    m_full_screen_quad = engine->CreateHandle<Mesh>(MeshBuilder::Quad().release());
    engine->InitObject(m_full_screen_quad);

    auto shader = engine->CreateHandle<Shader>(
        std::vector<SubShader> {
            {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/blit_vert.spv")).Read()}},
            {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/blit_frag.spv")).Read()}}
        }
    );

    engine->InitObject(shader);

    UInt iteration = 0;
    
    auto render_pass = engine->CreateHandle<RenderPass>(
        renderer::RenderPassStage::PRESENT,
        renderer::RenderPass::Mode::RENDER_PASS_INLINE
    );

    m_render_pass_attachments.push_back(std::make_unique<renderer::Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            engine->GetInstance()->swapchain->extent,
            engine->GetInstance()->swapchain->image_format,
            nullptr
        ),
        renderer::RenderPassStage::PRESENT
    ));

    m_render_pass_attachments.push_back(std::make_unique<renderer::Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            engine->GetInstance()->swapchain->extent,
            engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        renderer::RenderPassStage::PRESENT
    ));
    
    for (auto &attachment : m_render_pass_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetDevice()));
    }

    for (auto img : engine->GetInstance()->swapchain->images) {
        auto fbo = std::make_unique<Framebuffer>(
            engine->GetInstance()->swapchain->extent,
            Handle<RenderPass>(render_pass)
        );

        renderer::AttachmentRef *color_attachment_ref,
            *depth_attachment_ref;

        HYPERION_ASSERT_RESULT(m_render_pass_attachments[0]->AddAttachmentRef(
            engine->GetDevice(),
            img,
            renderer::Image::ToVkFormat(engine->GetInstance()->swapchain->image_format),
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D,
            1, 1,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &color_attachment_ref
        ));

        color_attachment_ref->SetBinding(0);

        fbo->GetFramebuffer().AddAttachmentRef(color_attachment_ref);

        HYPERION_ASSERT_RESULT(m_render_pass_attachments[1]->AddAttachmentRef(
            engine->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &depth_attachment_ref
        ));

        fbo->GetFramebuffer().AddAttachmentRef(depth_attachment_ref);

        depth_attachment_ref->SetBinding(1);

        if (iteration == 0) {
            render_pass->GetRenderPass().AddAttachmentRef(color_attachment_ref);
            render_pass->GetRenderPass().AddAttachmentRef(depth_attachment_ref);

            engine->InitObject(render_pass);

            m_renderer_instance = engine->CreateHandle<RendererInstance>(
                std::move(shader),
                Handle<RenderPass>(render_pass),
                RenderableAttributeSet(
                    MeshAttributes {
                        .vertex_attributes = renderer::static_mesh_vertex_attributes,
                        .fill_mode = FillMode::FILL
                    },
                    MaterialAttributes {
                        .bucket = BUCKET_SWAPCHAIN
                    }
                )
            );
        }

        m_renderer_instance->AddFramebuffer(engine->CreateHandle<Framebuffer>(fbo.release()));

        ++iteration;
    }

    engine->InitObject(m_renderer_instance);
}

void FinalPass::Destroy(Engine *engine)
{
    for (auto &attachment : m_render_pass_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Destroy(engine->GetDevice()));
    }
}

void FinalPass::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *pipeline = m_renderer_instance->GetPipeline();
    const UInt acquired_image_index = engine->GetInstance()->GetFrameHandler()->GetAcquiredImageIndex();

    m_renderer_instance->GetFramebuffers()[acquired_image_index]->BeginCapture(frame->GetCommandBuffer());
    
    pipeline->Bind(frame->GetCommandBuffer());

    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(),
        frame->GetCommandBuffer(),
        pipeline,
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

#if HYP_FEATURES_ENABLE_RAYTRACING
    /* TMP */
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetDevice(),
        command_buffer,
        pipeline,
        {{
            .set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
            .count = 1
        }}
    );
#endif

    /* Render full screen quad overlay to blit deferred + all post fx onto screen. */
    m_full_screen_quad->Render(engine, frame->GetCommandBuffer());
    
    m_renderer_instance->GetFramebuffers()[acquired_image_index]->EndCapture(frame->GetCommandBuffer());
}
} // namespace hyperion::v2
