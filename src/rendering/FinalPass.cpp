#include <rendering/FinalPass.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderPass.hpp>
#include <Engine.hpp>
#include <util/MeshBuilder.hpp>

#if 0
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

void FinalPass::Create()
{
    m_full_screen_quad = MeshBuilder::Quad();
    InitObject(m_full_screen_quad);

    auto shader = CreateObject<Shader>(
        std::vector<SubShader> {
            {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(Engine::Get()->GetAssetManager().GetBasePath().Data(), "vkshaders/blit_vert.spv")).Read()}},
            {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(Engine::Get()->GetAssetManager().GetBasePath().Data(), "vkshaders/blit_frag.spv")).Read()}}
        }
    );

    InitObject(shader);

    UInt iteration = 0;
    
    auto render_pass = CreateObject<RenderPass>(
        renderer::RenderPassStage::PRESENT,
        renderer::RenderPass::Mode::RENDER_PASS_INLINE
    );

    m_render_pass_attachments.push_back(std::make_unique<renderer::Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            Engine::Get()->GetGPUInstance()->swapchain->extent,
            Engine::Get()->GetGPUInstance()->swapchain->image_format,
            nullptr
        ),
        renderer::RenderPassStage::PRESENT
    ));

    m_render_pass_attachments.push_back(std::make_unique<renderer::Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            Engine::Get()->GetGPUInstance()->swapchain->extent,
            Engine::Get()->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        renderer::RenderPassStage::PRESENT
    ));
    
    for (auto &attachment : m_render_pass_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(Engine::Get()->GetGPUDevice()));
    }

    for (auto img : Engine::Get()->GetGPUInstance()->swapchain->images) {
        auto fbo = std::make_unique<Framebuffer>(
            Engine::Get()->GetGPUInstance()->swapchain->extent,
            Handle<RenderPass>(render_pass)
        );

        renderer::AttachmentRef *color_attachment_ref,
            *depth_attachment_ref;

        HYPERION_ASSERT_RESULT(m_render_pass_attachments[0]->AddAttachmentRef(
            Engine::Get()->GetGPUDevice(),
            img,
            renderer::Image::ToVkFormat(Engine::Get()->GetGPUInstance()->swapchain->image_format),
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D,
            1, 1,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &color_attachment_ref
        ));

        color_attachment_ref->SetBinding(0);

        fbo->GetFramebuffer().AddAttachmentRef(color_attachment_ref);

        HYPERION_ASSERT_RESULT(m_render_pass_attachments[1]->AddAttachmentRef(
            Engine::Get()->GetGPUDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &depth_attachment_ref
        ));

        fbo->GetFramebuffer().AddAttachmentRef(depth_attachment_ref);

        depth_attachment_ref->SetBinding(1);

        if (iteration == 0) {
            render_pass->GetRenderPass().AddAttachmentRef(color_attachment_ref);
            render_pass->GetRenderPass().AddAttachmentRef(depth_attachment_ref);

            InitObject(render_pass);

            m_render_group = CreateObject<RenderGroup>(
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

        m_render_group->AddFramebuffer(CreateObject<Framebuffer>(fbo.release()));

        ++iteration;
    }

    InitObject(m_render_group);
}

void FinalPass::Destroy()
{
    for (auto &attachment : m_render_pass_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Destroy(Engine::Get()->GetGPUDevice()));
    }
}

void FinalPass::Render(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *pipeline = m_render_group->GetPipeline();
    const UInt acquired_image_index = Engine::Get()->GetGPUInstance()->GetFrameHandler()->GetAcquiredImageIndex();

    m_render_group->GetFramebuffers()[acquired_image_index]->BeginCapture(frame->GetCommandBuffer());
    
    pipeline->Bind(frame->GetCommandBuffer());

    Engine::Get()->GetGPUInstance()->GetDescriptorPool().Bind(
        Engine::Get()->GetGPUDevice(),
        frame->GetCommandBuffer(),
        pipeline,
        {
            {.set = DescriptorSet::global_buffer_mapping[frame->GetFrameIndex()], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
        }
    );

#if HYP_FEATURES_ENABLE_RAYTRACING
    /* TMP */
    Engine::Get()->GetGPUInstance()->GetDescriptorPool().Bind(
        Engine::Get()->GetGPUDevice(),
        command_buffer,
        pipeline,
        {{
            .set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
            .count = 1
        }}
    );
#endif

    /* Render full screen quad overlay to blit deferred + all post fx onto screen. */
    m_full_screen_quad->Renderframe->GetCommandBuffer());
    
    m_render_group->GetFramebuffers()[acquired_image_index]->EndCapture(frame->GetCommandBuffer());
}
} // namespace hyperion::v2

#endif