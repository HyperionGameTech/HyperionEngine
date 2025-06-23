/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/GBuffer.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Deferred.hpp>

#include <rendering/backend/RenderingAPI.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererSwapchain.hpp>

#include <system/App.hpp>
#include <system/AppContext.hpp>

#include <core/threading/Threads.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region GBuffer

const FixedArray<GBufferResource, GBUFFER_RESOURCE_MAX> GBuffer::gbuffer_resources = {
    GBufferResource { GBufferFormat(DIF_COLOR) },   // color
    GBufferResource { GBufferFormat(DIF_NORMALS) }, // normal
    GBufferResource { GBufferFormat(TF_RGBA8) },    // material
    GBufferResource {                               // lightmap
        GBufferFormat(Array<TextureFormat> {
            TF_R11G11B10F,
            TF_RGBA16F }) },
    GBufferResource { GBufferFormat(TF_RG16F) }, // velocity
    GBufferResource {                            // objects mask
        GBufferFormat(Array<TextureFormat> {
            TF_R16 }) },
    GBufferResource { GBufferFormat(DIF_NORMALS) }, // world-space normals (untextured)
    GBufferResource { GBufferFormat(DIF_DEPTH) }    // depth
};

static TextureFormat GetImageFormat(GBufferResourceName resource)
{
    HYP_SCOPE;

    TextureFormat color_format = TF_NONE;

    if (const TextureFormat* format = GBuffer::gbuffer_resources[resource].format.TryGet<TextureFormat>())
    {
        color_format = *format;
    }
    else if (const DefaultImageFormat* default_format = GBuffer::gbuffer_resources[resource].format.TryGet<DefaultImageFormat>())
    {
        color_format = g_rendering_api->GetDefaultFormat(*default_format);
    }
    else if (const Array<TextureFormat>* default_formats = GBuffer::gbuffer_resources[resource].format.TryGet<Array<TextureFormat>>())
    {
        for (const TextureFormat format : *default_formats)
        {
            if (g_rendering_api->IsSupportedFormat(format, IS_SRV))
            {
                color_format = format;

                break;
            }
        }
    }

    AssertThrowMsg(color_format != TF_NONE, "Invalid value set for gbuffer image format");

    return color_format;
}

GBuffer::GBuffer(Vec2u extent)
    : m_extent(extent)
{
    for (uint32 bucket_index = 0; bucket_index < RB_MAX - 1; bucket_index++)
    {
        const RenderBucket rb = RenderBucket(bucket_index + 1);

        m_buckets[bucket_index].SetGBuffer(this);
        m_buckets[bucket_index].SetBucket(rb);
    }

    CreateBucketFramebuffers();
}

GBuffer::~GBuffer()
{
    for (GBufferBucket& it : m_buckets)
    {
        it.SetFramebuffer(nullptr);
    }

    SafeRelease(std::move(m_framebuffers));
}

void GBuffer::Create()
{
    HYP_SCOPE;

    HYP_LOG(Rendering, Debug, "Creating GBuffer with resolution {}", m_extent);

    for (auto& framebuffer : m_framebuffers)
    {
        DeferCreate(framebuffer);
    }
}

void GBuffer::Resize(Vec2u extent)
{
    HYP_SCOPE;

    if (m_extent == extent)
    {
        return;
    }

    m_extent = extent;

    for (GBufferBucket& it : m_buckets)
    {
        it.SetFramebuffer(nullptr);
    }

    SafeRelease(std::move(m_framebuffers));

    CreateBucketFramebuffers();

    for (auto& framebuffer : m_framebuffers)
    {
        DeferCreate(framebuffer);
    }

    OnGBufferResolutionChanged(m_extent);
}

void GBuffer::CreateBucketFramebuffers()
{
    HYP_SCOPE;

    AssertThrow(m_framebuffers.Empty());

    for (GBufferBucket& it : m_buckets)
    {
        const RenderBucket rb = it.GetBucket();

        switch (rb)
        {
        case RB_OPAQUE:
            it.m_framebuffer = CreateFramebuffer(nullptr, m_extent, rb);
            break;
        case RB_LIGHTMAP:
            it.m_framebuffer = CreateFramebuffer(GetBucket(RB_OPAQUE).m_framebuffer, m_extent, rb);
            break;
        case RB_TRANSLUCENT:
            it.m_framebuffer = CreateFramebuffer(GetBucket(RB_OPAQUE).m_framebuffer, m_extent, rb);
            break;
        case RB_SKYBOX:
            it.m_framebuffer = GetBucket(RB_TRANSLUCENT).m_framebuffer;
            break;
        case RB_DEBUG:
            it.m_framebuffer = GetBucket(RB_TRANSLUCENT).m_framebuffer;
            break;
        default:
            HYP_UNREACHABLE();
            break;
        }

        AssertThrow(it.m_framebuffer != nullptr);
    }
}

FramebufferRef GBuffer::CreateFramebuffer(const FramebufferRef& opaque_framebuffer, Vec2u resolution, RenderBucket rb)
{
    HYP_SCOPE;

    AssertThrow(resolution.Volume() != 0);

    FramebufferRef framebuffer = g_rendering_api->MakeFramebuffer(resolution);

    auto add_owned_attachment = [&](uint32 binding, TextureFormat format)
    {
        TextureDesc texture_desc;
        texture_desc.type = TT_TEX2D;
        texture_desc.format = format;
        texture_desc.extent = Vec3u { resolution, 1 };
        texture_desc.filter_mode_min = TFM_NEAREST;
        texture_desc.filter_mode_mag = TFM_NEAREST;
        texture_desc.wrap_mode = TWM_CLAMP_TO_EDGE;
        texture_desc.image_usage = IU_ATTACHMENT | IU_SAMPLED;

        framebuffer->AddAttachment(
            binding,
            g_rendering_api->MakeImage(texture_desc),
            LoadOperation::CLEAR,
            StoreOperation::STORE);
    };

    auto add_shared_attachment = [&](uint32 binding)
    {
        AttachmentBase* parent_attachment = opaque_framebuffer->GetAttachment(binding);
        AssertThrow(parent_attachment != nullptr);

        framebuffer->AddAttachment(
            binding,
            parent_attachment->GetImage(),
            LoadOperation::LOAD,
            StoreOperation::STORE);
    };

    // add gbuffer attachments
    // - The color attachment (first attachment) is unique to opaque and translucent buckets.
    //   The lightmap bucket will write into the opaque bucket's albedo attachment.
    // - The translucent bucket can potentially use a different format than
    //   the opaque bucket's albedo attachment.
    //   This is because the translucent bucket's framebuffer is used to render
    //   the shaded result in the deferred pass before the translucent objects are rendered
    //   using forward rendering, and we need to be able to accommodate the high range of
    //   values that can be produced by the deferred shading pass
    if (rb == RB_OPAQUE)
    {
        add_owned_attachment(0, GetImageFormat(GBUFFER_RESOURCE_ALBEDO));
    }
    else if (rb == RB_LIGHTMAP)
    {
        add_shared_attachment(0);
    }
    else
    {
        add_owned_attachment(0, TF_RGBA16F);
    }

    // opaque creates the main non-color gbuffer attachments,
    // which will be shared with other renderable buckets
    if (opaque_framebuffer == nullptr)
    {
        for (uint32 i = 1; i < GBUFFER_RESOURCE_MAX; i++)
        {
            const TextureFormat format = GetImageFormat(GBufferResourceName(i));

            add_owned_attachment(i, format);
        }
    }
    else
    {
        // add the attachments shared with opaque bucket
        for (uint32 i = 1; i < GBUFFER_RESOURCE_MAX; i++)
        {
            add_shared_attachment(i);
        }
    }

    m_framebuffers.PushBack(framebuffer);

    return framebuffer;
}

#pragma endregion GBuffer

#pragma region GBufferBucket

GBuffer::GBufferBucket::GBufferBucket()
{
}

GBuffer::GBufferBucket::~GBufferBucket()
{
}

AttachmentBase* GBuffer::GBufferBucket::GetGBufferAttachment(GBufferResourceName resource_name) const
{
    HYP_SCOPE;

    AssertThrow(m_framebuffer != nullptr);
    AssertThrow(uint32(resource_name) < uint32(GBUFFER_RESOURCE_MAX));

    return m_framebuffer->GetAttachment(uint32(resource_name));
}

#pragma endregion GBufferBucket

} // namespace hyperion
