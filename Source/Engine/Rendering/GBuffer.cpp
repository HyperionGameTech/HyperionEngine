/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/GBuffer.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/Swapchain.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <System/AppContext.hpp>

#include <Core/Threading/Threads.hpp>

#include <Framework/CVarManager.hpp>

#include <initializer_list>

#include <GBuffer.generated.inl>

namespace Hyperion {

extern CVar<bool> cvDepthPrepass;

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
        if (RI.IsSupportedFormat(*it, ImageSupport::Attachment))
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
    for (uint32 bucketIndex = 0; bucketIndex < NumRenderBuckets; bucketIndex++)
    {
        m_buckets[bucketIndex].SetGBuffer(this);
        m_buckets[bucketIndex].SetBucket(RenderBucket(bucketIndex));
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
        if (target.GetFramebuffer().IsValid())
        {
            FramebufferRef framebuffer = target.GetFramebuffer();
            target.SetFramebuffer(nullptr);

            EnqueueDeletion(std::move(framebuffer));
        }
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

    EnqueueDeletion(std::move(m_framebuffers));

    for (GBufferTarget& target : m_buckets)
    {
        const RenderBucket rb = target.GetBucket();

        switch (rb)
        {
        case RenderBucket::Opaque:
            target.m_framebuffer = CreateFramebuffer(nullptr, m_extent, rb);
            break;
        case RenderBucket::Lightmapped:    // fallthrough
        case RenderBucket::Translucent: // fallthrough
        case RenderBucket::Sky:      // fallthrough
        case RenderBucket::Debug:       // fallthrough
            target.m_framebuffer = CreateFramebuffer(GetBucket(RenderBucket::Opaque).m_framebuffer, m_extent, rb);
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

    FramebufferDesc framebufferDesc;
    framebufferDesc.extent = resolution;
    framebufferDesc.numLayers = 1;

    FramebufferRef framebuffer = RI.MakeFramebuffer(framebufferDesc);

#if HYP_DEBUG_MODE
    framebuffer->SetDebugName(NAME_FMT("{}Framebuffer", EnumToString(rb)));
#endif

    auto AddOwnedAttachment = [&](uint32 binding, TextureFormat format, LoadOperation loadOp = LoadOperation::CLEAR, StoreOperation storeOp = StoreOperation::STORE) -> Attachment*
    {
        return framebuffer->AddAttachment(
            binding,
            AttachmentDesc {
                TextureType::Texture2D,
                format,
                loadOp,
                storeOp
            });
    };

    auto AddSharedAttachment = [&](uint32 binding, LoadOperation loadOp = LoadOperation::LOAD, StoreOperation storeOp = StoreOperation::STORE) -> Attachment*
    {
        Assert(parentFramebuffer != nullptr);

        AttachmentBase* parentAttachment = parentFramebuffer->GetAttachment(binding);
        Assert(parentAttachment != nullptr);

        AttachmentDesc newDesc = parentAttachment->GetAttachmentDesc();
        newDesc.loadOp = loadOp;
        newDesc.storeOp = storeOp;

        return framebuffer->AddAttachment(
            binding,
            newDesc,
            RI.MakeImageView(parentAttachment->GetGpuImage()));
    };

    // add gbuffer attachments
    if (rb == RenderBucket::Opaque)
    {
        for (uint32 i = 0; i < GTN_DEPTH; i++)
        {
            const TextureFormat format = GetImageFormat(GBufferTargetName(i));

            AddOwnedAttachment(i, format);
        }

        if (cvDepthPrepass.Get())
        {
            // If DepthPrepass is enabled, we don't CLEAR the depth texture as DPP is responsible for clearing it.
            AddOwnedAttachment(GTN_DEPTH, GetImageFormat(GTN_DEPTH), LoadOperation::LOAD, StoreOperation::NONE);
        }
        else
        {
            // Otherwise, we clear it on render pass start.
            AddOwnedAttachment(GTN_DEPTH, GetImageFormat(GTN_DEPTH), LoadOperation::CLEAR, StoreOperation::STORE);
        }
    }
    else
    {
        Assert(parentFramebuffer != nullptr);

        // add the attachments shared with opaque bucket (including depth)
        for (uint32 i = 0; i < GTN_MAX; i++)
        {
            if (i == GTN_DEPTH)
            {
                if (rb == RenderBucket::Debug)
                {
                    // debug bucket creates its own depth attachment
                    const TextureFormat format = GetImageFormat(GBufferTargetName(i));
                    AddOwnedAttachment(i, format);

                    continue;
                }
                else if (rb == RenderBucket::Lightmapped && cvDepthPrepass.Get())
                {
                    // Lightmapped objects are included in the depth prepass, so we don't want to write to depth when they render.
                    // Therefore we use StoreOperation::NONE as storeOp when DepthPrepass is true.
                    AddSharedAttachment(i, LoadOperation::LOAD, StoreOperation::NONE);

                    continue;
                }
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
