#include "RenderList.hpp"

#include "../Engine.hpp"

namespace hyperion::v2 {

const std::array<TextureFormatDefault, num_gbuffer_textures> RenderListContainer::gbuffer_textures = {
    TEXTURE_FORMAT_DEFAULT_GBUFFER,//COLOR,   // color
    TEXTURE_FORMAT_DEFAULT_NORMALS, // normal
    TEXTURE_FORMAT_DEFAULT_UNUSED, // position -- No longer used. Reconstruct from depth instead.
    TEXTURE_FORMAT_DEFAULT_GBUFFER_8BIT, // material
    TEXTURE_FORMAT_DEFAULT_NORMALS, // tangent
    TEXTURE_FORMAT_DEFAULT_NORMALS, // bitangent
    TEXTURE_FORMAT_DEFAULT_DEPTH    // depth
};

RenderListContainer::RenderListContainer()
{
    for (size_t i = 0; i < m_buckets.Size(); i++) {
        m_buckets[i].SetBucket(Bucket(i));
    }
}

void RenderListContainer::AddFramebuffersToPipelines(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.AddFramebuffersToPipelines();
    }
}

void RenderListContainer::AddPendingRendererInstances(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.AddPendingRendererInstances(engine);
    }
}

void RenderListContainer::Create(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.CreateRenderPass(engine);
        bucket.CreateFramebuffers(engine);
    }
}

void RenderListContainer::Destroy(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.Destroy(engine);
    }
}

RenderListContainer::RenderListBucket::RenderListBucket()
{
}

RenderListContainer::RenderListBucket::~RenderListBucket()
{
}

void RenderListContainer::RenderListBucket::AddRendererInstance(Handle<RendererInstance> &&renderer_instance)
{
    AddFramebuffersToPipeline(renderer_instance);

    std::lock_guard guard(renderer_instances_mutex);

    renderer_instances_pending_addition.PushBack(std::move(renderer_instance));
    renderer_instances_changed.Set(true);
}

void RenderListContainer::RenderListBucket::AddPendingRendererInstances(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!renderer_instances_changed.Get()) {
        return;
    }

    DebugLog(LogType::Debug, "Adding %llu pending graphics pipelines\n", renderer_instances_pending_addition.Size());

    std::lock_guard guard(renderer_instances_mutex);
    DebugLog(LogType::Debug, "Adding pending graphics pipelines, locked mutex.\n");

    for (auto it = renderer_instances_pending_addition.Begin(); it != renderer_instances_pending_addition.End(); ++it) {
        AssertThrow(*it != nullptr);

        engine->Attach(*it);

        renderer_instances.PushBack(std::move(*it));
    }
    
    renderer_instances_pending_addition = {};
    renderer_instances_changed.Set(false);
}

void RenderListContainer::RenderListBucket::AddFramebuffersToPipelines()
{
    for (auto &pipeline : renderer_instances) {
        AddFramebuffersToPipeline(pipeline);
    }
}

void RenderListContainer::RenderListBucket::AddFramebuffersToPipeline(Handle<RendererInstance> &pipeline)
{
    for (auto &framebuffer : framebuffers) {
        pipeline->AddFramebuffer(Handle<Framebuffer>(framebuffer));
    }
}

void RenderListContainer::RenderListBucket::CreateRenderPass(Engine *engine)
{
    AssertThrow(render_pass == nullptr);

    auto mode = renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER;

    if (bucket == BUCKET_SWAPCHAIN) {
        mode = renderer::RenderPass::Mode::RENDER_PASS_INLINE;
    }
    
    render_pass = Handle<RenderPass>(new RenderPass(
        RenderPassStage::SHADER,
        mode
    ));

    if (IsRenderableBucket()) { // add gbuffer attachments
        AttachmentRef *attachment_ref;

        auto framebuffer_image = std::make_unique<renderer::FramebufferImage2D>(
            engine->GetInstance()->swapchain->extent,
            engine->GetDefaultFormat(TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_GBUFFER),//engine->GetDefaultFormat(gbuffer_textures[0]),
            nullptr
        );
        
        attachments.PushBack(std::make_unique<renderer::Attachment>(
            std::move(framebuffer_image),
            RenderPassStage::SHADER
        ));

        HYPERION_ASSERT_RESULT(attachments.Back()->AddAttachmentRef(
            engine->GetInstance()->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &attachment_ref
        ));

        render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

        for (UInt i = 1; i < gbuffer_textures.size() - 1 /* because color and depth already accounted for*/; i++) {
            auto framebuffer_image = std::make_unique<renderer::FramebufferImage2D>(
                engine->GetInstance()->swapchain->extent,
                engine->GetDefaultFormat(gbuffer_textures[i]),
                nullptr
            );

            attachments.PushBack(std::make_unique<renderer::Attachment>(
                std::move(framebuffer_image),
                RenderPassStage::SHADER
            ));

            HYPERION_ASSERT_RESULT(attachments.Back()->AddAttachmentRef(
                engine->GetInstance()->GetDevice(),
                renderer::LoadOperation::CLEAR,
                renderer::StoreOperation::STORE,
                &attachment_ref
            ));

            render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);
        }

        constexpr SizeType depth_texture_index = gbuffer_textures.size() - 1;

        /* Add depth attachment */
        if (bucket == BUCKET_TRANSLUCENT) { // translucent reuses the opaque bucket's depth buffer.
            auto &forward_fbo = engine->GetRenderListContainer()[BUCKET_OPAQUE].framebuffers[0];
            AssertThrow(forward_fbo != nullptr);

            renderer::AttachmentRef *depth_attachment;

            HYPERION_ASSERT_RESULT(forward_fbo->GetFramebuffer().GetAttachmentRefs().at(depth_texture_index)->AddAttachmentRef(
                engine->GetInstance()->GetDevice(),
                renderer::StoreOperation::STORE,
                &depth_attachment
            ));

            depth_attachment->SetBinding(depth_texture_index);

            render_pass->GetRenderPass().AddAttachmentRef(depth_attachment);
        } else {
            attachments.PushBack(std::make_unique<renderer::Attachment>(
                std::make_unique<renderer::FramebufferImage2D>(
                    engine->GetInstance()->swapchain->extent,
                    engine->GetDefaultFormat(gbuffer_textures[depth_texture_index]),
                    nullptr
                ),
                RenderPassStage::SHADER
            ));

            HYPERION_ASSERT_RESULT(attachments.Back()->AddAttachmentRef(
                engine->GetInstance()->GetDevice(),
                renderer::LoadOperation::CLEAR,
                renderer::StoreOperation::STORE,
                &attachment_ref
            ));

            render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);
        }
    }

    for (auto &attachment : attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetInstance()->GetDevice()));
    }
    
    engine->Attach(render_pass);
}

void RenderListContainer::RenderListBucket::CreateFramebuffers(Engine *engine)
{
    AssertThrow(framebuffers.Empty());
    
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto framebuffer = Handle<Framebuffer>(new Framebuffer(engine->GetInstance()->swapchain->extent, Handle<RenderPass>(render_pass)));

        for (auto *attachment_ref : render_pass->GetRenderPass().GetAttachmentRefs()) {
            framebuffer->GetFramebuffer().AddAttachmentRef(attachment_ref);
        }

        engine->Attach(framebuffer);
        framebuffers.PushBack(framebuffer);
    }
}

void RenderListContainer::RenderListBucket::Destroy(Engine *engine)
{
    auto result = renderer::Result::OK;

    renderer_instances.Clear();

    renderer_instances_mutex.lock();
    renderer_instances_pending_addition.Clear();
    renderer_instances_mutex.unlock();

    framebuffers.Clear();

    for (const auto &attachment : attachments) {
        HYPERION_PASS_ERRORS(
            attachment->Destroy(engine->GetInstance()->GetDevice()),
            result
        );
    }

    HYPERION_ASSERT_RESULT(result);
}

} // namespace hyperion::v2
