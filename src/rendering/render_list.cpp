#include "render_list.h"

#include "../engine.h"

namespace hyperion::v2 {

const std::array<TextureFormatDefault, 5> RenderListContainer::gbuffer_textures = {
    TEXTURE_FORMAT_DEFAULT_COLOR,
    TEXTURE_FORMAT_DEFAULT_NORMALS,
    TEXTURE_FORMAT_DEFAULT_GBUFFER,
    TEXTURE_FORMAT_DEFAULT_GBUFFER,
    TEXTURE_FORMAT_DEFAULT_DEPTH
};

RenderListContainer::RenderListContainer()
{
    for (size_t i = 0; i < m_buckets.size(); i++) {
        m_buckets[i].SetBucket(Bucket(i));
    }
}

void RenderListContainer::AddFramebuffersToPipelines(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.AddFramebuffersToPipelines();
    }
}

void RenderListContainer::AddPendingGraphicsPipelines(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.AddPendingGraphicsPipelines(engine);
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
    : m_graphics_pipeline_notifier([this]() -> std::vector<std::pair<Ref<GraphicsPipeline> *, size_t>> {
          //std::lock_guard guard(graphics_pipelines_mutex);

          return {
              std::make_pair(graphics_pipelines.data(), graphics_pipelines.size()),
              std::make_pair(graphics_pipelines_pending_addition.data(), graphics_pipelines_pending_addition.size())
          };
      })
{
}

RenderListContainer::RenderListBucket::~RenderListBucket()
{
}

void RenderListContainer::RenderListBucket::AddGraphicsPipeline(Ref<GraphicsPipeline> &&graphics_pipeline)
{
    AddFramebuffersToPipeline(graphics_pipeline);

    graphics_pipeline.Init();

    std::lock_guard guard(graphics_pipelines_mutex);

    graphics_pipelines_pending_addition.push_back(std::move(graphics_pipeline));
    graphics_pipelines_changed = true;
}

void RenderListContainer::RenderListBucket::AddPendingGraphicsPipelines(Engine *engine)
{
    Engine::AssertOnThread(THREAD_RENDER);

    if (!graphics_pipelines_changed) {
        return;
    }

    DebugLog(LogType::Debug, "Adding %llu pending graphics pipelines\n", graphics_pipelines_pending_addition.size());

    std::lock_guard guard(graphics_pipelines_mutex);

    graphics_pipelines.reserve(graphics_pipelines.size() + graphics_pipelines_pending_addition.size());
    
    m_graphics_pipeline_notifier.ItemsAdded(graphics_pipelines_pending_addition.data(), graphics_pipelines_pending_addition.size());

    graphics_pipelines.insert(
        graphics_pipelines.end(),
        std::make_move_iterator(graphics_pipelines_pending_addition.begin()),
        std::make_move_iterator(graphics_pipelines_pending_addition.end())
    );

    graphics_pipelines_pending_addition.clear();

    graphics_pipelines_changed = false;
}

void RenderListContainer::RenderListBucket::AddFramebuffersToPipelines()
{
    for (auto &pipeline : graphics_pipelines) {
        AddFramebuffersToPipeline(pipeline);
    }
}

void RenderListContainer::RenderListBucket::AddFramebuffersToPipeline(Ref<GraphicsPipeline> &pipeline)
{
    for (auto &framebuffer : framebuffers) {
        pipeline->AddFramebuffer(framebuffer.IncRef());
    }
}

void RenderListContainer::RenderListBucket::CreateRenderPass(Engine *engine)
{
    AssertThrow(render_pass == nullptr);

    auto mode = renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER;

    if (bucket == BUCKET_SWAPCHAIN) {
        mode = renderer::RenderPass::Mode::RENDER_PASS_INLINE;
    }
    
    render_pass = engine->resources.render_passes.Add(std::make_unique<RenderPass>(
        RenderPassStage::SHADER,
        mode
    ));

    if (IsRenderableBucket()) { // add gbuffer attachments
        AttachmentRef *attachment_ref;

        attachments.push_back(std::make_unique<renderer::Attachment>(
            std::make_unique<renderer::FramebufferImage2D>(
                engine->GetInstance()->swapchain->extent,
                engine->GetDefaultFormat(gbuffer_textures[0]),
                nullptr
            ),
            RenderPassStage::SHADER
        ));

        HYPERION_ASSERT_RESULT(attachments.back()->AddAttachmentRef(
            engine->GetInstance()->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &attachment_ref
        ));

        render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

        for (uint i = 1; i < gbuffer_textures.size() - 1 /* because color and depth already accounted for*/; i++) {
            attachments.push_back(std::make_unique<renderer::Attachment>(
                std::make_unique<renderer::FramebufferImage2D>(
                    engine->GetInstance()->swapchain->extent,
                    engine->GetDefaultFormat(gbuffer_textures[i]),
                    nullptr
                ),
                RenderPassStage::SHADER
            ));

            HYPERION_ASSERT_RESULT(attachments.back()->AddAttachmentRef(
                engine->GetInstance()->GetDevice(),
                renderer::LoadOperation::CLEAR,
                renderer::StoreOperation::STORE,
                &attachment_ref
            ));

            render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);
        }

        constexpr size_t depth_texture_index = gbuffer_textures.size() - 1;

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
            attachments.push_back(std::make_unique<renderer::Attachment>(
                std::make_unique<renderer::FramebufferImage2D>(
                    engine->GetInstance()->swapchain->extent,
                    engine->GetDefaultFormat(gbuffer_textures[depth_texture_index]),
                    nullptr
                ),
                RenderPassStage::SHADER
            ));

            HYPERION_ASSERT_RESULT(attachments.back()->AddAttachmentRef(
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
    
    render_pass.Init();
}

void RenderListContainer::RenderListBucket::CreateFramebuffers(Engine *engine)
{
    AssertThrow(framebuffers.empty());

    const uint num_frames = engine->GetInstance()->GetFrameHandler()->NumFrames();

    for (uint i = 0; i < 1/*num_frames*/; i++) {
        auto framebuffer = std::make_unique<Framebuffer>(engine->GetInstance()->swapchain->extent, render_pass.IncRef());

        for (auto *attachment_ref : render_pass->GetRenderPass().GetAttachmentRefs()) {
            framebuffer->GetFramebuffer().AddAttachmentRef(attachment_ref);
        }

        framebuffers.push_back(engine->resources.framebuffers.Add(
            std::move(framebuffer)
        ));

        framebuffers.back().Init();
    }
}

void RenderListContainer::RenderListBucket::Destroy(Engine *engine)
{
    auto result = renderer::Result::OK;

    for (auto &pipeline : graphics_pipelines) {
        m_graphics_pipeline_notifier.ItemRemoved(pipeline);
    }

    graphics_pipelines.clear();

    graphics_pipelines_mutex.lock();

    for (auto &pipeline : graphics_pipelines_pending_addition) {
        m_graphics_pipeline_notifier.ItemRemoved(pipeline);
    }

    graphics_pipelines_pending_addition.clear();

    graphics_pipelines_mutex.unlock();

    framebuffers.clear();

    for (const auto &attachment : attachments) {
        HYPERION_PASS_ERRORS(
            attachment->Destroy(engine->GetInstance()->GetDevice()),
            result
        );
    }

    HYPERION_ASSERT_RESULT(result);
}

} // namespace hyperion::v2
