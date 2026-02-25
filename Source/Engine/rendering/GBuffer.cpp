/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/GBuffer.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/Swapchain.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <system/AppContext.hpp>

#include <Core/threading/Threads.hpp>

#include <GBuffer.generated.inl>

namespace Hyperion {

#pragma region GBuffer

struct GBufferTargetDesc
{
    TextureFormat formats[4];

    GBufferTargetDesc(std::initializer_list<TextureFormat> formatsList)
    {
        Assert(formatsList.size() <= std::size(formats));

        for (uint32 i = 0; i < uint32(formatsList.size()); i++)
        {
            formats[i] = *(formatsList.begin() + i);
        }

        for (uint32 i = uint32(formatsList.size()); i < uint32(std::size(formats)); i++)
        {
            formats[i] = InvalidTextureFormat;
        }
    }
};

static const FixedArray<GBufferTargetDesc, GTN_MAX> s_targetDescs = {
    GBufferTargetDesc { TextureFormat::RGBA16F },                     // color
    GBufferTargetDesc { TextureFormat::R10G10B10A2 },                 // normal: https://johnwhite3d.blogspot.com/2017/10/signed-octahedron-normal-encoding.html
    GBufferTargetDesc { TextureFormat::RGBA32 },                      // material data
    GBufferTargetDesc { TextureFormat::RG16F },                       // velocity
    GBufferTargetDesc { TextureFormat::D24_S8, TextureFormat::D32F_S8 } // depth
};

static TextureFormat GetImageFormat(GBufferTargetName targetName)
{
    for (auto it = std::begin(s_targetDescs[targetName].formats); it != std::end(s_targetDescs[targetName].formats) && *it != InvalidTextureFormat; ++it)
    {
        if (g_renderInterface->IsSupportedFormat(*it, ImageSupport::Attachment))
        {
            return *it;
        }
    }

    HYP_FAIL("Failed to find supported image format for gbuffer target {}", EnumToString(targetName));

    return InvalidTextureFormat;
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
}

GBuffer::~GBuffer()
{
    for (GBufferTarget& it : m_buckets)
    {
        it.SetFramebuffer(nullptr);
    }

    EnqueueDeletion(std::move(m_framebuffers));
}

void GBuffer::Create()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (m_isCreated)
    {
        return;
    }

    HYP_LOG(Rendering, Verbose, "Creating GBuffer with resolution {}", m_extent);

    CreateBucketFramebuffers();

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

    HYP_LOG(Rendering, Verbose, "Resizing GBuffer from {}x{} to {}x{}", m_extent.x, m_extent.y, extent.x, extent.y);

    m_extent = extent;

    for (GBufferTarget& target : m_buckets)
    {
        target.SetFramebuffer(nullptr);
    }

    EnqueueDeletion(std::move(m_framebuffers));

    CreateBucketFramebuffers();

    if (m_isCreated)
    {
        for (const FramebufferRef& framebuffer : m_framebuffers)
        {
            CheckResult(framebuffer->Create());
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

    RenderTargetDesc renderTargetDesc;
    renderTargetDesc.extent = resolution;
    renderTargetDesc.numLayers = 1;

    FramebufferRef framebuffer = g_renderInterface->MakeFramebuffer(renderTargetDesc);

    auto AddOwnedAttachment = [&](uint32 binding, TextureFormat format) -> Attachment*
    {
        TextureDesc textureDesc;
        textureDesc.type = TextureType::Texture2D;
        textureDesc.format = format;
        textureDesc.extent = Vec3u { resolution, 1 };
        textureDesc.filterModeMin = TFM_NEAREST;
        textureDesc.filterModeMag = TFM_NEAREST;
        textureDesc.wrapMode = TWM_CLAMP_TO_EDGE;
        textureDesc.imageUsage = IU_ATTACHMENT | IU_SAMPLED;

        GpuImageRef gpuImage = g_renderInterface->MakeImage(textureDesc);
        
#ifdef HYP_DEBUG_MODE
        gpuImage->SetDebugName(NAME_FMT("GBufferTarget_{}_{}", binding, EnumToString(rb)));
#endif

        return framebuffer->AddAttachment(
            binding,
            gpuImage,
            LoadOperation::CLEAR,
            StoreOperation::STORE);
    };

    auto AddSharedAttachment = [&](uint32 binding) -> Attachment*
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

            AddOwnedAttachment(i, format);
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
                AddOwnedAttachment(i, format);

                continue;
            }

            AddSharedAttachment(i);
        }
    }

    CheckResult(framebuffer->Create());

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
