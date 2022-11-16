#include <rendering/DeferredSystem.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

const FixedArray<GBufferResource, GBUFFER_RESOURCE_MAX> DeferredSystem::gbuffer_resources = {
    GBufferResource { GBufferFormat(TEXTURE_FORMAT_DEFAULT_COLOR) }, // color
    GBufferResource { GBufferFormat(TEXTURE_FORMAT_DEFAULT_NORMALS) }, // normal
    GBufferResource { GBufferFormat(InternalFormat::RGBA8) }, // material
    GBufferResource { GBufferFormat(InternalFormat::RGBA8) }, // tangent
    GBufferResource { GBufferFormat(InternalFormat::RG16F) }, // velocity
    GBufferResource {  // objects mask
        GBufferFormat(Array<InternalFormat> {
            //InternalFormat::R32,
            InternalFormat::R16
        })
    },
    GBufferResource { GBufferFormat(TEXTURE_FORMAT_DEFAULT_DEPTH) } // depth
};

static void AddOwnedAttachment(
    Engine *engine,
    InternalFormat format,
    Handle<RenderPass> &render_pass,
    Array<std::unique_ptr<Attachment>> &attachments
)
{
    AttachmentRef *attachment_ref;

    auto framebuffer_image = std::make_unique<renderer::FramebufferImage2D>(
        engine->GetInstance()->swapchain->extent,
        format,
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

static void AddSharedAttachment(
    Engine *engine,
    UInt attachment_index,
    Handle<RenderPass> &render_pass,
    Array<std::unique_ptr<Attachment>> &attachments
)
{
    auto &opaque_fbo = engine->GetDeferredSystem()[BUCKET_OPAQUE].GetFramebuffers()[0];
    AssertThrow(opaque_fbo != nullptr);

    renderer::AttachmentRef *attachment_ref;

    AssertThrow(attachment_index < opaque_fbo->GetFramebuffer().GetAttachmentRefs().size());

    HYPERION_ASSERT_RESULT(opaque_fbo->GetFramebuffer().GetAttachmentRefs().at(attachment_index)->AddAttachmentRef(
        engine->GetInstance()->GetDevice(),
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    attachment_ref->SetBinding(attachment_index);

    render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);
}

static InternalFormat GetImageFormat(Engine *engine, GBufferResourceName resource)
{
    InternalFormat color_format;

    if (const InternalFormat *format = DeferredSystem::gbuffer_resources[resource].format.TryGet<InternalFormat>()) {
        color_format = *format;
    } else if (const TextureFormatDefault *default_format = DeferredSystem::gbuffer_resources[resource].format.TryGet<TextureFormatDefault>()) {
        color_format = engine->GetDefaultFormat(*default_format);   
    } else if (const Array<InternalFormat> *default_formats = DeferredSystem::gbuffer_resources[resource].format.TryGet<Array<InternalFormat>>()) {
        for (const InternalFormat format : *default_formats) {
            if (engine->GetDevice()->GetFeatures().IsSupportedFormat(format, renderer::ImageSupportType::SRV)) {
                color_format = format;

                break;
            }
        }
    } else {
        AssertThrowMsg(false, "Invalid value set for gbuffer image format");
    }

    return color_format;
}

DeferredSystem::DeferredSystem()
{
    for (SizeType i = 0; i < m_buckets.Size(); i++) {
        m_buckets[i].SetBucket(Bucket(i));
    }
}

void DeferredSystem::AddFramebuffersToPipelines(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.AddFramebuffersToPipelines();
    }
}

void DeferredSystem::AddPendingRendererInstances(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.AddPendingRendererInstances(engine);
    }
}

void DeferredSystem::Create(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.CreateRenderPass(engine);
        bucket.CreateFramebuffers(engine);
    }
}

void DeferredSystem::Destroy(Engine *engine)
{
    for (auto &bucket : m_buckets) {
        bucket.Destroy(engine);
    }
}

DeferredSystem::RendererInstanceHolder::RendererInstanceHolder()
{
}

DeferredSystem::RendererInstanceHolder::~RendererInstanceHolder()
{
}

void DeferredSystem::RendererInstanceHolder::AddRendererInstance(Handle<RendererInstance> &renderer_instance)
{
    AddFramebuffersToPipeline(renderer_instance);

    std::lock_guard guard(renderer_instances_mutex);

    renderer_instances_pending_addition.PushBack(renderer_instance);
    renderer_instances_changed.Set(true);

    DebugLog(
        LogType::Debug,
        "Add RendererInstance (current count: %llu, pending: %llu)\n",
        renderer_instances.Size(),
        renderer_instances_pending_addition.Size()
    );
}

void DeferredSystem::RendererInstanceHolder::AddPendingRendererInstances(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!renderer_instances_changed.Get()) {
        return;
    }

    DebugLog(LogType::Debug, "Adding %llu pending RendererInstances\n", renderer_instances_pending_addition.Size());

    std::lock_guard guard(renderer_instances_mutex);
    DebugLog(LogType::Debug, "Adding pending RendererInstances, locked mutex.\n");

    for (auto it = renderer_instances_pending_addition.Begin(); it != renderer_instances_pending_addition.End(); ++it) {
        AssertThrow(*it != nullptr);

        engine->InitObject(*it);

        renderer_instances.PushBack(std::move(*it));
    }
    
    renderer_instances_pending_addition.Clear();
    renderer_instances_changed.Set(false);
}

void DeferredSystem::RendererInstanceHolder::AddFramebuffersToPipelines()
{
    for (auto &pipeline : renderer_instances) {
        AddFramebuffersToPipeline(pipeline);
    }
}

void DeferredSystem::RendererInstanceHolder::AddFramebuffersToPipeline(Handle<RendererInstance> &pipeline)
{
    for (auto &framebuffer : framebuffers) {
        pipeline->AddFramebuffer(Handle<Framebuffer>(framebuffer));
    }
}

void DeferredSystem::RendererInstanceHolder::CreateRenderPass(Engine *engine)
{
    AssertThrow(render_pass == nullptr);

    auto mode = renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER;

    if (bucket == BUCKET_SWAPCHAIN) {
        mode = renderer::RenderPass::Mode::RENDER_PASS_INLINE;
    }
    
    render_pass = engine->CreateHandle<RenderPass>(
        RenderPassStage::SHADER,
        mode
    );

    const InternalFormat color_format = GetImageFormat(engine, GBUFFER_RESOURCE_ALBEDO);

    if (bucket == BUCKET_UI) {
        // ui only has this attachment.
        AddOwnedAttachment(
            engine,
            color_format,
            render_pass,
            attachments
        );
    } else if (BucketIsRenderable(bucket)) {
        // add gbuffer attachments
        // color attachment is unique for all buckets
        AddOwnedAttachment(
            engine,
            color_format,
            render_pass,
            attachments
        );

        // opaque creates the main non-color gbuffer attachments,
        // which will be shared with other renderable buckets
        if (bucket == BUCKET_OPAQUE) {
            for (UInt i = 1; i < GBUFFER_RESOURCE_MAX; i++) {
                const InternalFormat format = GetImageFormat(engine, GBufferResourceName(i));

                AddOwnedAttachment(
                    engine,
                    format,
                    render_pass,
                    attachments
                );
            }
        } else {
            // add the attachments shared with opaque bucket
            for (UInt i = 1; i < GBUFFER_RESOURCE_MAX; i++) {
                AddSharedAttachment(
                    engine,
                    i,
                    render_pass,
                    attachments
                );
            }
        }
    }

    for (const auto &attachment : attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetInstance()->GetDevice()));
    }
    
    engine->InitObject(render_pass);
}

void DeferredSystem::RendererInstanceHolder::CreateFramebuffers(Engine *engine)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto framebuffer = engine->CreateHandle<Framebuffer>(
            engine->GetInstance()->swapchain->extent,
            Handle<RenderPass>(render_pass)
        );

        for (auto *attachment_ref : render_pass->GetRenderPass().GetAttachmentRefs()) {
            framebuffer->GetFramebuffer().AddAttachmentRef(attachment_ref);
        }

        engine->InitObject(framebuffer);
        framebuffers[frame_index] = std::move(framebuffer);
    }
}

void DeferredSystem::RendererInstanceHolder::Destroy(Engine *engine)
{
    auto result = renderer::Result::OK;

    renderer_instances.Clear();

    renderer_instances_mutex.lock();
    renderer_instances_pending_addition.Clear();
    renderer_instances_mutex.unlock();

    framebuffers = { };

    for (const auto &attachment : attachments) {
        HYPERION_PASS_ERRORS(
            attachment->Destroy(engine->GetInstance()->GetDevice()),
            result
        );
    }

    HYPERION_ASSERT_RESULT(result);
}

} // namespace hyperion::v2
