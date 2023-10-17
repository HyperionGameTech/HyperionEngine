#include <rendering/DeferredSystem.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

const FixedArray<GBufferResource, GBUFFER_RESOURCE_MAX> DeferredSystem::gbuffer_resources = {
    GBufferResource { GBufferFormat(TEXTURE_FORMAT_DEFAULT_COLOR) }, // color
    GBufferResource { GBufferFormat(TEXTURE_FORMAT_DEFAULT_NORMALS) }, // normal
    GBufferResource { GBufferFormat(InternalFormat::RGBA8) }, // material
    GBufferResource { GBufferFormat(InternalFormat::RGBA16F) }, // tangent
    GBufferResource { GBufferFormat(InternalFormat::RG16F) }, // velocity
    GBufferResource {  // objects mask
        GBufferFormat(Array<InternalFormat> {
            InternalFormat::R16
        })
    },
    GBufferResource { GBufferFormat(TEXTURE_FORMAT_DEFAULT_NORMALS) }, // world-space normals (untextured)
    GBufferResource { GBufferFormat(TEXTURE_FORMAT_DEFAULT_DEPTH) } // depth
};

static void AddOwnedAttachment(
    InternalFormat format,
    Handle<Framebuffer> &framebuffer,
    Array<std::unique_ptr<Attachment>> &attachments,
    Extent2D extent = Extent2D { 0, 0 }
)
{
    if (!extent.Size()) {
        extent = g_engine->GetGPUInstance()->swapchain->extent;
    }

    AttachmentUsage *attachment_usage;

    attachments.PushBack(std::make_unique<renderer::Attachment>(
        MakeRenderObject<Image>(renderer::FramebufferImage2D(
            extent,
            format,
            nullptr
        )),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(attachments.Back()->AddAttachmentUsage(
        g_engine->GetGPUInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_usage
    ));

    // ALLOW alpha blending if a pipeline uses it, not neccessarily enabling it.
    attachment_usage->SetAllowBlending(true);

    framebuffer->AddAttachmentUsage(attachment_usage);
}

static void AddSharedAttachment(
    UInt attachment_index,
    Handle<Framebuffer> &framebuffer,
    Array<std::unique_ptr<Attachment>> &attachments
)
{
    auto &opaque_fbo = g_engine->GetDeferredSystem()[BUCKET_OPAQUE].GetFramebuffer();
    AssertThrowMsg(opaque_fbo != nullptr, "Bucket framebuffers added in wrong order");

    renderer::AttachmentUsage *attachment_usage;

    AssertThrow(attachment_index < opaque_fbo->GetAttachmentUsages().size());

    HYPERION_ASSERT_RESULT(opaque_fbo->GetAttachmentUsages().at(attachment_index)->AddAttachmentUsage(
        g_engine->GetGPUInstance()->GetDevice(),
        renderer::StoreOperation::STORE,
        &attachment_usage
    ));

    attachment_usage->SetBinding(attachment_index);

    // if (attachment_index == 5) {//opaque_fbo->GetAttachmentUsages().at(attachment_index)->IsDepthAttachment()) {
        attachment_usage->SetAllowBlending(false);
    // }

    framebuffer->AddAttachmentUsage(attachment_usage);
}

static InternalFormat GetImageFormat(GBufferResourceName resource)
{
    InternalFormat color_format = InternalFormat::NONE;

    if (const InternalFormat *format = DeferredSystem::gbuffer_resources[resource].format.TryGet<InternalFormat>()) {
        color_format = *format;
    } else if (const TextureFormatDefault *default_format = DeferredSystem::gbuffer_resources[resource].format.TryGet<TextureFormatDefault>()) {
        color_format = g_engine->GetDefaultFormat(*default_format);   
    } else if (const Array<InternalFormat> *default_formats = DeferredSystem::gbuffer_resources[resource].format.TryGet<Array<InternalFormat>>()) {
        for (const InternalFormat format : *default_formats) {
            if (g_engine->GetGPUDevice()->GetFeatures().IsSupportedFormat(format, renderer::ImageSupportType::SRV)) {
                color_format = format;

                break;
            }
        }
    }

    AssertThrowMsg(color_format != InternalFormat::NONE, "Invalid value set for gbuffer image format");

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

void DeferredSystem::RenderGroupHolder::AddRenderGroup(Handle<RenderGroup> &render_group)
{
    if (render_group->GetRenderableAttributes().GetFramebufferID()) {
        Handle<Framebuffer> framebuffer(render_group->GetRenderableAttributes().GetFramebufferID());

        AssertThrowMsg(framebuffer.IsValid(), "Invalid framebuffer ID %u", render_group->GetRenderableAttributes().GetFramebufferID().Value());

        render_group->AddFramebuffer(std::move(framebuffer));
    } else {
        AddFramebuffersToPipeline(render_group);
    }

    //InitObject(render_group);

    std::lock_guard guard(renderer_instances_mutex);

    renderer_instances_pending_addition.PushBack(render_group);
    renderer_instances_changed.Set(true, MemoryOrder::RELEASE);

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

    if (!renderer_instances_changed.Get(MemoryOrder::ACQUIRE)) {
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
    renderer_instances_changed.Set(false, MemoryOrder::RELEASE);
}

void DeferredSystem::RenderGroupHolder::AddFramebuffersToPipelines()
{
    for (auto &pipeline : renderer_instances) {
        AddFramebuffersToPipeline(pipeline);
    }
}

void DeferredSystem::RenderGroupHolder::AddFramebuffersToPipeline(Handle<RenderGroup> &pipeline)
{
    pipeline->AddFramebuffer(Handle<Framebuffer>(m_framebuffer));
}

void DeferredSystem::RenderGroupHolder::CreateFramebuffer()
{
    auto mode = renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER;

    if (bucket == BUCKET_SWAPCHAIN) {
        mode = renderer::RenderPass::Mode::RENDER_PASS_INLINE;
    }

    const Extent2D extent = g_engine->GetGPUInstance()->swapchain->extent;

    m_framebuffer = CreateObject<Framebuffer>(
        extent,
        RenderPassStage::SHADER,
        mode
    );

    const InternalFormat color_format = GetImageFormat(GBUFFER_RESOURCE_ALBEDO);

    if (bucket == BUCKET_UI) {
        // ui only has this attachment.
        AddOwnedAttachment(
            color_format,
            m_framebuffer,
            attachments,
            extent
        );
    } else if (BucketIsRenderable(bucket)) {
        // add gbuffer attachments
        // color attachment is unique for all buckets
        AddOwnedAttachment(
            color_format,
            m_framebuffer,
            attachments,
            extent
        );

        // opaque creates the main non-color gbuffer attachments,
        // which will be shared with other renderable buckets
        if (bucket == BUCKET_OPAQUE) {
            for (UInt i = 1; i < GBUFFER_RESOURCE_MAX; i++) {
                const InternalFormat format = GetImageFormat(GBufferResourceName(i));

                AddOwnedAttachment(
                    format,
                    m_framebuffer,
                    attachments,
                    extent
                );
            }
        } else {
            // add the attachments shared with opaque bucket
            for (UInt i = 1; i < GBUFFER_RESOURCE_MAX; i++) {
                AddSharedAttachment(
                    i,
                    m_framebuffer,
                    attachments
                );
            }
        }
    }

    for (const auto &attachment : attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(g_engine->GetGPUInstance()->GetDevice()));
    }

    InitObject(m_framebuffer);
}

void DeferredSystem::RenderGroupHolder::Destroy()
{
    auto result = renderer::Result::OK;

    renderer_instances.Clear();

    renderer_instances_mutex.lock();
    renderer_instances_pending_addition.Clear();
    renderer_instances_mutex.unlock();

    m_framebuffer.Reset();

    for (const auto &attachment : attachments) {
        HYPERION_PASS_ERRORS(
            attachment->Destroy(g_engine->GetGPUInstance()->GetDevice()),
            result
        );
    }

    HYPERION_ASSERT_RESULT(result);
}

} // namespace hyperion::v2
