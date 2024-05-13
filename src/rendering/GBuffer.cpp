/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/GBuffer.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <Engine.hpp>

namespace hyperion {

const FixedArray<GBufferResource, GBUFFER_RESOURCE_MAX> GBuffer::gbuffer_resources = {
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
    uint binding,
    InternalFormat format,
    FramebufferRef &framebuffer,
    Extent2D extent = Extent2D { 0, 0 }
)
{
    if (!extent.Size()) {
        extent = g_engine->GetGPUInstance()->GetSwapchain()->extent;
    }

    ImageRef attachment_image = MakeRenderObject<Image>(renderer::FramebufferImage2D(
        extent,
        format,
        nullptr
    ));

    HYPERION_ASSERT_RESULT(attachment_image->Create(g_engine->GetGPUInstance()->GetDevice()));

    AttachmentRef attachment = MakeRenderObject<renderer::Attachment>(
        attachment_image,
        renderer::RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );
    
    attachment->SetBinding(binding);

    // if (!attachment_image->IsDepthStencil()) {
    //     attachment->SetAllowBlending(true);
    // }

    HYPERION_ASSERT_RESULT(attachment->Create(g_engine->GetGPUInstance()->GetDevice()));

    framebuffer->AddAttachment(std::move(attachment));
}

static void AddSharedAttachment(
    uint binding,
    FramebufferRef &framebuffer
)
{
    const FramebufferRef &opaque_fbo = g_engine->GetGBuffer().Get(BUCKET_OPAQUE).GetFramebuffer();
    AssertThrowMsg(opaque_fbo.IsValid(), "Bucket framebuffers added in wrong order");

    const AttachmentRef &parent_attachment = opaque_fbo->GetAttachment(binding);
    AssertThrow(parent_attachment.IsValid());
    
    AttachmentRef attachment = MakeRenderObject<renderer::Attachment>(
        parent_attachment->GetImage(),
        renderer::RenderPassStage::SHADER,
        renderer::LoadOperation::LOAD,
        renderer::StoreOperation::STORE
    );

    attachment->SetBinding(binding);
    attachment->SetAllowBlending(false);
    HYPERION_ASSERT_RESULT(attachment->Create(g_engine->GetGPUInstance()->GetDevice()));

    framebuffer->AddAttachment(attachment);
}

static InternalFormat GetImageFormat(GBufferResourceName resource)
{
    InternalFormat color_format = InternalFormat::NONE;

    if (const InternalFormat *format = GBuffer::gbuffer_resources[resource].format.TryGet<InternalFormat>()) {
        color_format = *format;
    } else if (const TextureFormatDefault *default_format = GBuffer::gbuffer_resources[resource].format.TryGet<TextureFormatDefault>()) {
        color_format = g_engine->GetDefaultFormat(*default_format);   
    } else if (const Array<InternalFormat> *default_formats = GBuffer::gbuffer_resources[resource].format.TryGet<Array<InternalFormat>>()) {
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

GBuffer::GBuffer()
{
    for (SizeType i = 0; i < m_buckets.Size(); i++) {
        m_buckets[i].SetBucket(Bucket(i));
    }
}

void GBuffer::Create()
{
    for (auto &bucket : m_buckets) {
        bucket.CreateFramebuffer();
    }
}

void GBuffer::Destroy()
{
    for (auto &bucket : m_buckets) {
        bucket.Destroy();
    }
}

GBuffer::GBufferBucket::GBufferBucket()
{
}

GBuffer::GBufferBucket::~GBufferBucket()
{
}

const AttachmentRef &GBuffer::GBufferBucket::GetGBufferAttachment(GBufferResourceName resource_name) const
{
    AssertThrow(framebuffer != nullptr);
    AssertThrow(uint(resource_name) < uint(GBUFFER_RESOURCE_MAX));

    return framebuffer->GetAttachment(uint(resource_name));
}

void GBuffer::GBufferBucket::AddRenderGroup(Handle<RenderGroup> &render_group)
{
    if (render_group->GetRenderableAttributes().GetFramebuffer().IsValid()) {
        const FramebufferRef &framebuffer = render_group->GetRenderableAttributes().GetFramebuffer();
        AssertThrowMsg(framebuffer != nullptr, "Invalid framebuffer");

        render_group->AddFramebuffer(framebuffer);
    } else {
        AddFramebuffersToRenderGroup(render_group);
    }
}

void GBuffer::GBufferBucket::AddFramebuffersToRenderGroup(Handle<RenderGroup> &render_group)
{
    render_group->AddFramebuffer(framebuffer);
}

void GBuffer::GBufferBucket::CreateFramebuffer()
{
    renderer::RenderPassMode mode = renderer::RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER;

    if (bucket == BUCKET_SWAPCHAIN) {
        mode = renderer::RenderPassMode::RENDER_PASS_INLINE;
    }

    Extent2D extent = g_engine->GetGPUInstance()->GetSwapchain()->extent;

    // if (bucket == BUCKET_UI) {
    //     extent = {2000, 2000};//tmp
    // }

    framebuffer = MakeRenderObject<renderer::Framebuffer>(
        extent,
        renderer::RenderPassStage::SHADER,
        mode
    );

    const InternalFormat color_format = GetImageFormat(GBUFFER_RESOURCE_ALBEDO);

    if (bucket == BUCKET_UI) {
        AddOwnedAttachment(
            0,
            InternalFormat::RGBA8_SRGB,
            framebuffer,
            extent
        );

        // Needed for stencil
        AddOwnedAttachment(
            1,
            InternalFormat::DEPTH_32F,
            framebuffer,
            extent
        );
    } else if (BucketIsRenderable(bucket)) {
        // add gbuffer attachments
        // color attachment is unique for all buckets
        AddOwnedAttachment(
            0,
            color_format,
            framebuffer,
            extent
        );

        // opaque creates the main non-color gbuffer attachments,
        // which will be shared with other renderable buckets
        if (bucket == BUCKET_OPAQUE) {
            for (uint i = 1; i < GBUFFER_RESOURCE_MAX; i++) {
                const InternalFormat format = GetImageFormat(GBufferResourceName(i));

                AddOwnedAttachment(
                    i,
                    format,
                    framebuffer,
                    extent
                );
            }
        } else {
            // add the attachments shared with opaque bucket
            for (uint i = 1; i < GBUFFER_RESOURCE_MAX; i++) {
                AddSharedAttachment(
                    i,
                    framebuffer
                );
            }
        }
    }

    DeferCreate(framebuffer, g_engine->GetGPUDevice());
}

void GBuffer::GBufferBucket::Destroy()
{
    framebuffer.Reset();
}

} // namespace hyperion
