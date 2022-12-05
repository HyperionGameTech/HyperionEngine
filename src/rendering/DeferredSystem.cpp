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
    InternalFormat format,
    Handle<Framebuffer2> &framebuffer,
    Array<std::unique_ptr<Attachment>> &attachments
)
{
    AttachmentRef *attachment_ref;

    auto framebuffer_image = std::make_unique<renderer::FramebufferImage2D>(
        Engine::Get()->GetGPUInstance()->swapchain->extent,
        format,
        nullptr
    );

    attachments.PushBack(std::make_unique<renderer::Attachment>(
        std::move(framebuffer_image),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(attachments.Back()->AddAttachmentRef(
        Engine::Get()->GetGPUInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    framebuffer->AddAttachmentRef(attachment_ref);
}

static void AddSharedAttachment(
    UInt attachment_index,
    Handle<Framebuffer2> &framebuffer,
    Array<std::unique_ptr<Attachment>> &attachments
)
{
    auto &opaque_fbo = Engine::Get()->GetDeferredSystem()[BUCKET_OPAQUE].GetFramebuffer();
    AssertThrow(opaque_fbo != nullptr);

    renderer::AttachmentRef *attachment_ref;

    AssertThrow(attachment_index < opaque_fbo->GetAttachmentRefs().size());

    HYPERION_ASSERT_RESULT(opaque_fbo->GetAttachmentRefs().at(attachment_index)->AddAttachmentRef(
        Engine::Get()->GetGPUInstance()->GetDevice(),
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    attachment_ref->SetBinding(attachment_index);

    framebuffer->AddAttachmentRef(attachment_ref);
}

static InternalFormat GetImageFormat(GBufferResourceName resource)
{
    InternalFormat color_format;

    if (const InternalFormat *format = DeferredSystem::gbuffer_resources[resource].format.TryGet<InternalFormat>()) {
        color_format = *format;
    } else if (const TextureFormatDefault *default_format = DeferredSystem::gbuffer_resources[resource].format.TryGet<TextureFormatDefault>()) {
        color_format = Engine::Get()->GetDefaultFormat(*default_format);   
    } else if (const Array<InternalFormat> *default_formats = DeferredSystem::gbuffer_resources[resource].format.TryGet<Array<InternalFormat>>()) {
        for (const InternalFormat format : *default_formats) {
            if (Engine::Get()->GetGPUDevice()->GetFeatures().IsSupportedFormat(format, renderer::ImageSupportType::SRV)) {
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

void DeferredSystem::AddFramebuffersToPipelines()
{
    for (auto &bucket : m_buckets) {
        bucket.AddFramebuffersToPipelines();
    }
}

void DeferredSystem::AddPendingRenderGroups()
{
    for (auto &bucket : m_buckets) {
        bucket.AddPendingRenderGroups();
    }
}

void DeferredSystem::Create()
{
    for (auto &bucket : m_buckets) {
        bucket.CreateFramebuffer();
    }
}

void DeferredSystem::Destroy()
{
    for (auto &bucket : m_buckets) {
        bucket.Destroy();
    }
}

DeferredSystem::RenderGroupHolder::RenderGroupHolder()
{
}

DeferredSystem::RenderGroupHolder::~RenderGroupHolder()
{
}

void DeferredSystem::RenderGroupHolder::AddRenderGroup(Handle<RenderGroup> &renderer_instance)
{
    AddFramebuffersToPipeline(renderer_instance);

    std::lock_guard guard(renderer_instances_mutex);

    renderer_instances_pending_addition.PushBack(renderer_instance);
    renderer_instances_changed.Set(true);

    DebugLog(
        LogType::Debug,
        "Add RenderGroup (current count: %llu, pending: %llu)\n",
        renderer_instances.Size(),
        renderer_instances_pending_addition.Size()
    );
}

void DeferredSystem::RenderGroupHolder::AddPendingRenderGroups()
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (!renderer_instances_changed.Get()) {
        return;
    }

    DebugLog(LogType::Debug, "Adding %llu pending RenderGroups\n", renderer_instances_pending_addition.Size());

    std::lock_guard guard(renderer_instances_mutex);
    DebugLog(LogType::Debug, "Adding pending RenderGroups, locked mutex.\n");

    for (auto it = renderer_instances_pending_addition.Begin(); it != renderer_instances_pending_addition.End(); ++it) {
        AssertThrow(*it != nullptr);

        InitObject(*it);

        renderer_instances.PushBack(std::move(*it));
    }
    
    renderer_instances_pending_addition.Clear();
    renderer_instances_changed.Set(false);
}

void DeferredSystem::RenderGroupHolder::AddFramebuffersToPipelines()
{
    for (auto &pipeline : renderer_instances) {
        AddFramebuffersToPipeline(pipeline);
    }
}

void DeferredSystem::RenderGroupHolder::AddFramebuffersToPipeline(Handle<RenderGroup> &pipeline)
{
    pipeline->AddFramebuffer(Handle<Framebuffer2>(framebuffer));
}

void DeferredSystem::RenderGroupHolder::CreateFramebuffer()
{
    auto mode = renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER;

    if (bucket == BUCKET_SWAPCHAIN) {
        mode = renderer::RenderPass::Mode::RENDER_PASS_INLINE;
    }

    framebuffer = CreateObject<Framebuffer2>(
        Engine::Get()->GetGPUInstance()->swapchain->extent,
        RenderPassStage::SHADER,
        mode
    );

    const InternalFormat color_format = GetImageFormat(GBUFFER_RESOURCE_ALBEDO);

    if (bucket == BUCKET_UI) {
        // ui only has this attachment.
        AddOwnedAttachment(
            color_format,
            framebuffer,
            attachments
        );
    } else if (BucketIsRenderable(bucket)) {
        // add gbuffer attachments
        // color attachment is unique for all buckets
        AddOwnedAttachment(
            color_format,
            framebuffer,
            attachments
        );

        // opaque creates the main non-color gbuffer attachments,
        // which will be shared with other renderable buckets
        if (bucket == BUCKET_OPAQUE) {
            for (UInt i = 1; i < GBUFFER_RESOURCE_MAX; i++) {
                const InternalFormat format = GetImageFormat(GBufferResourceName(i));

                AddOwnedAttachment(
                    format,
                    framebuffer,
                    attachments
                );
            }
        } else {
            // add the attachments shared with opaque bucket
            for (UInt i = 1; i < GBUFFER_RESOURCE_MAX; i++) {
                AddSharedAttachment(
                    i,
                    framebuffer,
                    attachments
                );
            }
        }
    }

    for (const auto &attachment : attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(Engine::Get()->GetGPUInstance()->GetDevice()));
    }

    InitObject(framebuffer);
}

void DeferredSystem::RenderGroupHolder::Destroy()
{
    auto result = renderer::Result::OK;

    renderer_instances.Clear();

    renderer_instances_mutex.lock();
    renderer_instances_pending_addition.Clear();
    renderer_instances_mutex.unlock();

    framebuffer.Reset();

    for (const auto &attachment : attachments) {
        HYPERION_PASS_ERRORS(
            attachment->Destroy(Engine::Get()->GetGPUInstance()->GetDevice()),
            result
        );
    }

    HYPERION_ASSERT_RESULT(result);
}

} // namespace hyperion::v2
