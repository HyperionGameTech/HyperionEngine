/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/GBuffer.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Deferred.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderSwapchain.hpp>

#include <system/App.hpp>
#include <system/AppContext.hpp>

#include <core/threading/Threads.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

#pragma region GBuffer

struct GBufferTargetDesc
{
    GBufferFormat format;
};

static const FixedArray<GBufferTargetDesc, GTN_MAX> g_targetDescs = {
    GBufferTargetDesc { GBufferFormat(Array<TextureFormat> { TF_R11G11B10F }) }, // color
    GBufferTargetDesc { GBufferFormat(DIF_NORMALS) },                            // normal
    GBufferTargetDesc { GBufferFormat(TF_RG32) },                                // material data
    GBufferTargetDesc { GBufferFormat(Array<TextureFormat> { TF_R11G11B10F, TF_RGBA16F }) },
    GBufferTargetDesc { GBufferFormat(TF_RG16F) },    // velocity
    GBufferTargetDesc { GBufferFormat(DIF_NORMALS) }, // world-space normals (untextured)
    GBufferTargetDesc { GBufferFormat(DIF_DEPTH) }    // depth
};

static TextureFormat GetImageFormat(GBufferTargetName targetName)
{
    HYP_SCOPE;

    TextureFormat colorFormat = TF_NONE;

    if (const TextureFormat* format = g_targetDescs[targetName].format.TryGet<TextureFormat>())
    {
        colorFormat = *format;
    }
    else if (const DefaultImageFormat* defaultFormat = g_targetDescs[targetName].format.TryGet<DefaultImageFormat>())
    {
        colorFormat = g_renderBackend->GetDefaultFormat(*defaultFormat);
    }
    else if (const Array<TextureFormat>* defaultFormats = g_targetDescs[targetName].format.TryGet<Array<TextureFormat>>())
    {
        for (const TextureFormat format : *defaultFormats)
        {
            if (g_renderBackend->IsSupportedFormat(format, IS_SRV))
            {
                colorFormat = format;

                break;
            }
        }
    }

    Assert(colorFormat != TF_NONE, "Invalid value set for gbuffer image format");

    return colorFormat;
}

GBuffer::GBuffer(Vec2u extent)
    : m_extent(extent),
      m_isCreated(false)
{
    for (uint32 bucketIndex = 0; bucketIndex < RB_MAX - 1; bucketIndex++)
    {
        const RenderBucket rb = RenderBucket(bucketIndex + 1);

        m_buckets[bucketIndex].SetGBuffer(this);
        m_buckets[bucketIndex].SetBucket(rb);
    }

    CreateBucketFramebuffers();
}

GBuffer::~GBuffer()
{
    for (GBufferTarget& it : m_buckets)
    {
        it.SetFramebuffer(nullptr);
    }

    SafeRelease(std::move(m_framebuffers));
}

void GBuffer::Create()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    if (m_isCreated)
    {
        return;
    }

    HYP_LOG(Rendering, Debug, "Creating GBuffer with resolution {}", m_extent);

    for (auto& framebuffer : m_framebuffers)
    {
        HYP_GFX_ASSERT(framebuffer->Create());
    }

    m_isCreated = true;
}

void GBuffer::Resize(Vec2u extent)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    if (m_extent == extent)
    {
        return;
    }

    m_extent = extent;

    for (GBufferTarget& it : m_buckets)
    {
        it.SetFramebuffer(nullptr);
    }

    SafeRelease(std::move(m_framebuffers));

    CreateBucketFramebuffers();

    if (m_isCreated)
    {
        for (auto& framebuffer : m_framebuffers)
        {
            HYP_GFX_ASSERT(framebuffer->Create());
        }

        OnGBufferResolutionChanged(m_extent);
    }
}

void GBuffer::CreateBucketFramebuffers()
{
    HYP_SCOPE;
    
    m_framebuffers.Clear();

    for (GBufferTarget& it : m_buckets)
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

        Assert(it.m_framebuffer != nullptr);
    }
}

FramebufferRef GBuffer::CreateFramebuffer(const FramebufferRef& opaqueFramebuffer, Vec2u resolution, RenderBucket rb)
{
    HYP_SCOPE;

    Assert(resolution.Volume() != 0);

    FramebufferRef framebuffer = g_renderBackend->MakeFramebuffer(resolution);

    auto addOwnedAttachment = [&](uint32 binding, TextureFormat format)
    {
        TextureDesc textureDesc;
        textureDesc.type = TT_TEX2D;
        textureDesc.format = format;
        textureDesc.extent = Vec3u { resolution, 1 };
        textureDesc.filterModeMin = TFM_NEAREST;
        textureDesc.filterModeMag = TFM_NEAREST;
        textureDesc.wrapMode = TWM_CLAMP_TO_EDGE;
        textureDesc.imageUsage = IU_ATTACHMENT | IU_SAMPLED;

        framebuffer->AddAttachment(
            binding,
            g_renderBackend->MakeImage(textureDesc),
            LoadOperation::CLEAR,
            StoreOperation::STORE);
    };

    auto addSharedAttachment = [&](uint32 binding)
    {
        AttachmentBase* parentAttachment = opaqueFramebuffer->GetAttachment(binding);
        Assert(parentAttachment != nullptr);

        framebuffer->AddAttachment(
            binding,
            parentAttachment->GetImage(),
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
        addOwnedAttachment(0, GetImageFormat(GTN_ALBEDO));
    }
    else if (rb == RB_LIGHTMAP)
    {
        addSharedAttachment(0);
    }
    else
    {
        addOwnedAttachment(0, TF_RGBA16F);
    }

    // opaque creates the main non-color gbuffer attachments,
    // which will be shared with other renderable buckets
    if (opaqueFramebuffer == nullptr)
    {
        for (uint32 i = 1; i < GTN_MAX; i++)
        {
            const TextureFormat format = GetImageFormat(GBufferTargetName(i));

            addOwnedAttachment(i, format);
        }
    }
    else
    {
        // add the attachments shared with opaque bucket
        for (uint32 i = 1; i < GTN_MAX; i++)
        {
            addSharedAttachment(i);
        }
    }

    m_framebuffers.PushBack(framebuffer);

    return framebuffer;
}

#pragma endregion GBuffer

#pragma region GBufferTarget

GBuffer::GBufferTarget::GBufferTarget()
{
}

GBuffer::GBufferTarget::~GBufferTarget()
{
}

AttachmentBase* GBuffer::GBufferTarget::GetGBufferAttachment(GBufferTargetName resourceName) const
{
    HYP_SCOPE;

    Assert(m_framebuffer != nullptr);
    Assert(uint32(resourceName) < uint32(GTN_MAX));

    return m_framebuffer->GetAttachment(uint32(resourceName));
}

#pragma endregion GBufferTarget

} // namespace hyperion
