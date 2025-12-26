/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/GBuffer.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/Swapchain.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <system/App.hpp>
#include <system/AppContext.hpp>

#include <core/threading/Threads.hpp>

#include <GBuffer.generated.inl>

namespace Hyperion {

#pragma region GBuffer

struct GBufferTargetDesc
{
    GBufferFormat format;
};

static const FixedArray<GBufferTargetDesc, GTN_MAX> s_targetDescs = {
    GBufferTargetDesc { GBufferFormat(TF_RGBA16F) },     // color
    GBufferTargetDesc { GBufferFormat(TF_R10G10B10A2) }, // normal: https://johnwhite3d.blogspot.com/2017/10/signed-octahedron-normal-encoding.html
    GBufferTargetDesc { GBufferFormat(TF_RGBA32) },      // material data
    GBufferTargetDesc { GBufferFormat(TF_RG16F) },       // velocity
    GBufferTargetDesc { GBufferFormat(TF_DEPTH_32F) }    // depth
};

static TextureFormat GetImageFormat(GBufferTargetName targetName)
{
    HYP_SCOPE;

    TextureFormat colorFormat = TF_NONE;

    if (const TextureFormat* format = s_targetDescs[targetName].format.TryGet<TextureFormat>())
    {
        colorFormat = *format;
    }
    else if (const DefaultImageFormat* defaultFormat = s_targetDescs[targetName].format.TryGet<DefaultImageFormat>())
    {
        colorFormat = g_renderBackend->GetDefaultFormat(*defaultFormat);
    }
    else if (const Array<TextureFormat>* defaultFormats = s_targetDescs[targetName].format.TryGet<Array<TextureFormat>>())
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

    SafeDelete(std::move(m_framebuffers));
}

void GBuffer::Create()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (m_isCreated)
    {
        return;
    }

    HYP_LOG(Rendering, Debug, "Creating GBuffer with resolution {}", m_extent);

    for (const FramebufferRef& framebuffer : m_framebuffers)
    {
        HYP_GFX_ASSERT(framebuffer->Create());
    }

    m_isCreated = true;
}

void GBuffer::Resize(Vec2u extent)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (m_extent == extent)
    {
        return;
    }

    HYP_LOG(Rendering, Debug, "Resizing GBuffer from {}x{} to {}x{}", m_extent.x, m_extent.y, extent.x, extent.y);

    m_extent = extent;

    for (GBufferTarget& target : m_buckets)
    {
        target.SetFramebuffer(nullptr);
    }

    SafeDelete(std::move(m_framebuffers));

    CreateBucketFramebuffers();

    if (m_isCreated)
    {
        for (const FramebufferRef& framebuffer : m_framebuffers)
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

    for (GBufferTarget& target : m_buckets)
    {
        const RenderBucket rb = target.GetBucket();

        switch (rb)
        {
        case RB_OPAQUE:
            target.m_framebuffer = CreateFramebuffer(nullptr, m_extent, rb);
            break;
        case RB_LIGHTMAP:    // fallthrough
        case RB_TRANSLUCENT: // fallthrough
        case RB_SKYBOX:      // fallthrough
        case RB_DEBUG:       // fallthrough
            target.m_framebuffer = CreateFramebuffer(GetBucket(RB_OPAQUE).m_framebuffer, m_extent, rb);
            break;
        default:
            HYP_UNREACHABLE();
            break;
        }

        Assert(target.m_framebuffer != nullptr);
    }
}

FramebufferRef GBuffer::CreateFramebuffer(const FramebufferRef& parentFramebuffer, Vec2u resolution, RenderBucket rb)
{
    HYP_SCOPE;

    Assert(resolution.Volume() != 0);

    FramebufferRef framebuffer = g_renderBackend->MakeFramebuffer(resolution);

    auto addOwnedAttachment = [&](uint32 binding, TextureFormat format) -> AttachmentRef
    {
        TextureDesc textureDesc;
        textureDesc.type = TT_TEX2D;
        textureDesc.format = format;
        textureDesc.extent = Vec3u { resolution, 1 };
        textureDesc.filterModeMin = TFM_NEAREST;
        textureDesc.filterModeMag = TFM_NEAREST;
        textureDesc.wrapMode = TWM_CLAMP_TO_EDGE;
        textureDesc.imageUsage = IU_ATTACHMENT | IU_SAMPLED;

        GpuImageRef gpuImage = g_renderBackend->MakeImage(textureDesc);
        gpuImage->SetDebugName(NAME_FMT("GBufferTarget_{}_{}", binding, EnumToString(rb)));

        return framebuffer->AddAttachment(
            binding,
            gpuImage,
            LoadOperation::CLEAR,
            StoreOperation::STORE);
    };

    auto addSharedAttachment = [&](uint32 binding) -> AttachmentRef
    {
        Assert(parentFramebuffer != nullptr);

        AttachmentBase* parentAttachment = parentFramebuffer->GetAttachment(binding);
        Assert(parentAttachment != nullptr);

        return framebuffer->AddAttachment(
            binding,
            parentAttachment->GetImage(),
            LoadOperation::LOAD,
            StoreOperation::STORE);
    };

    // add gbuffer attachments
    if (rb == RB_OPAQUE)
    {
        for (uint32 i = 0; i < GTN_MAX; i++)
        {
            const TextureFormat format = GetImageFormat(GBufferTargetName(i));

            addOwnedAttachment(i, format);
        }
    }
    else
    {
        Assert(parentFramebuffer != nullptr);

        // add the attachments shared with opaque bucket (including depth)
        for (uint32 i = 0; i < GTN_MAX; i++)
        {
            if (rb == RB_DEBUG && i == GTN_DEPTH)
            {
                // debug bucket creates its own depth attachment
                const TextureFormat format = GetImageFormat(GBufferTargetName(i));
                addOwnedAttachment(i, format);

                continue;
            }

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

} // namespace Hyperion
