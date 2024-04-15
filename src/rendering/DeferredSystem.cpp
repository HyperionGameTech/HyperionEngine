/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/DeferredSystem.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

const FixedArray<GBufferResource, GBUFFER_RESOURCE_MAX> DeferredSystem::gbuffer_resources = {
    GBufferResource { GBufferFormat(TEXTURE_FORMAT_DEFAULT_COLOR) }, // color
    GBufferResource { GBufferFormat(TEXTURE_FORMAT_DEFAULT_NORMALS) }, // normal
    GBufferResource { GBufferFormat(InternalFormat::RGBA8) }, // material
    GBufferResource { GBufferFormat(InternalFormat::RGBA16F) }, // tangent, bitangent
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
    Array<AttachmentRef> &attachments,
    Extent2D extent = Extent2D { 0, 0 }
)
{
    if (!extent.Size()) {
        extent = g_engine->GetGPUInstance()->GetSwapchain()->extent;
    }

    auto attachment = MakeRenderObject<renderer::Attachment>(
        MakeRenderObject<Image>(renderer::FramebufferImage2D(
            extent,
            format,
            nullptr
        )),
        RenderPassStage::SHADER
    );

    HYPERION_ASSERT_RESULT(attachment->Create(g_engine->GetGPUInstance()->GetDevice()));
    attachments.PushBack(attachment);

    auto attachment_usage = MakeRenderObject<renderer::AttachmentUsage>(
        attachment,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    // ALLOW alpha blending if a pipeline uses it, not neccessarily enabling it.
    attachment_usage->SetAllowBlending(true);
    HYPERION_ASSERT_RESULT(attachment_usage->Create(g_engine->GetGPUInstance()->GetDevice()));
    framebuffer->AddAttachmentUsage(std::move(attachment_usage));
}

static void AddSharedAttachment(
    uint attachment_index,
    Handle<Framebuffer> &framebuffer,
    Array<AttachmentRef> &attachments
)
{
    auto &opaque_fbo = g_engine->GetDeferredSystem()[BUCKET_OPAQUE].GetFramebuffer();
    AssertThrowMsg(opaque_fbo != nullptr, "Bucket framebuffers added in wrong order");

    AssertThrow(attachment_index < opaque_fbo->GetAttachmentUsages().Size());

    auto attachment_usage = MakeRenderObject<renderer::AttachmentUsage>(
        opaque_fbo->GetAttachmentUsages()[attachment_index]->GetAttachment(),
        renderer::LoadOperation::LOAD,
        renderer::StoreOperation::STORE
    );

    attachment_usage->SetBinding(attachment_index);
    attachment_usage->SetAllowBlending(false);
    HYPERION_ASSERT_RESULT(attachment_usage->Create(g_engine->GetGPUInstance()->GetDevice()));

    framebuffer->AddAttachmentUsage(std::move(attachment_usage));
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

void DeferredSystem::AddFramebuffersToRenderGroups()
{
    for (auto &bucket : m_buckets) {
        bucket.AddFramebuffersToRenderGroups();
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
        AddFramebuffersToRenderGroup(render_group);
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

void DeferredSystem::RenderGroupHolder::AddFramebuffersToRenderGroups()
{
    for (auto &render_group : renderer_instances) {
        AddFramebuffersToRenderGroup(render_group);
    }
}

void DeferredSystem::RenderGroupHolder::AddFramebuffersToRenderGroup(Handle<RenderGroup> &render_group)
{
    render_group->AddFramebuffer(Handle<Framebuffer>(framebuffer));
}

void DeferredSystem::RenderGroupHolder::CreateFramebuffer()
{
    renderer::RenderPassMode mode = renderer::RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER;

    if (bucket == BUCKET_SWAPCHAIN) {
        mode = renderer::RenderPassMode::RENDER_PASS_INLINE;
    }

    Extent2D extent = g_engine->GetGPUInstance()->GetSwapchain()->extent;

    if (bucket == BUCKET_UI) {
        extent = {2000, 2000};//tmp
    }

    framebuffer = CreateObject<Framebuffer>(
        extent,
        RenderPassStage::SHADER,
        mode
    );

    const InternalFormat color_format = GetImageFormat(GBUFFER_RESOURCE_ALBEDO);

    if (bucket == BUCKET_UI) {
        // ui only has this attachment.
        AddOwnedAttachment(
            InternalFormat::RGBA8_SRGB,
            framebuffer,
            attachments,
            extent
        );
    } else if (BucketIsRenderable(bucket)) {
        // add gbuffer attachments
        // color attachment is unique for all buckets
        AddOwnedAttachment(
            color_format,
            framebuffer,
            attachments,
            extent
        );

        // opaque creates the main non-color gbuffer attachments,
        // which will be shared with other renderable buckets
        if (bucket == BUCKET_OPAQUE) {
            for (uint i = 1; i < GBUFFER_RESOURCE_MAX; i++) {
                const InternalFormat format = GetImageFormat(GBufferResourceName(i));

                AddOwnedAttachment(
                    format,
                    framebuffer,
                    attachments,
                    extent
                );
            }
        } else {
            // add the attachments shared with opaque bucket
            for (uint i = 1; i < GBUFFER_RESOURCE_MAX; i++) {
                AddSharedAttachment(
                    i,
                    framebuffer,
                    attachments
                );
            }
        }
    }

    InitObject(framebuffer);
}

void DeferredSystem::RenderGroupHolder::Destroy()
{
    renderer_instances.Clear();

    renderer_instances_mutex.lock();
    renderer_instances_pending_addition.Clear();
    renderer_instances_mutex.unlock();

    framebuffer.Reset();

    for (AttachmentRef &attachment : attachments) {
        SafeRelease(std::move(attachment));
    }
}

} // namespace hyperion::v2
