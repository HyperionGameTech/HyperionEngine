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

extern CVar<bool> g_cvDepthPrepass;

static constexpr const char* TargetNameStrings[GBufferTarget::Max] = {
    "Color",
    "Normals",
    "MatData",
    "Velocity",
    "Depth"
};

struct GBufferTargetDesc
{
    static constexpr size_t MaxFormats = 4;

    TextureFormat formats[MaxFormats];

    template <size_t N>
    HYP_CONSTEVAL GBufferTargetDesc(const TextureFormat (&inFormats)[N])
    {
        static_assert(N <= MaxFormats);

        for (size_t i = 0; i < N; i++)
        {
            formats[i] = *(inFormats + i);
        }

        for (size_t i = N; i < MaxFormats; i++)
        {
            formats[i] = InvalidTextureFormat;
        }
    }
};

static constexpr GBufferTargetDesc TargetDescs[GBufferTarget::Max] = {
    GBufferTargetDesc({ TextureFormat::RGBA16F }),                       // Albedo
    GBufferTargetDesc({ TextureFormat::R10G10B10A2 }),                   // Normals: https://johnwhite3d.blogspot.com/2017/10/signed-octahedron-normal-encoding.html
    GBufferTargetDesc({ TextureFormat::R32 }),                           // MatData
    GBufferTargetDesc({ TextureFormat::RG16F }),                         // Velocity
    GBufferTargetDesc({ TextureFormat::D24_S8, TextureFormat::D32F_S8 }) // Depth
};

static inline TextureFormat GetImageFormat(GBufferTarget::TargetName targetName)
{
    for (auto it = std::begin(TargetDescs[targetName].formats); it != std::end(TargetDescs[targetName].formats) && *it != InvalidTextureFormat; ++it)
    {
        if (RI.IsSupportedFormat(*it, ImageSupport::Attachment))
        {
            return *it;
        }
    }

    HYP_FAIL("Failed to find supported image format for gbuffer target {}", TargetNameStrings[targetName]);

    return InvalidTextureFormat;
}

#pragma region GBuffer

GBuffer::GBuffer(Vec2u extent)
    : m_extent(extent),
      m_isCreated(false)
{
    for (uint8 pass = 0; pass < NumGBufferPasses; pass++)
    {
        m_passes[pass].gbuffer = this;
        m_passes[pass].pass = static_cast<GBufferPass>(pass);
    }
}

GBuffer::~GBuffer()
{
    for (GBufferTarget& it : m_passes)
    {
        it.framebuffer = FramebufferRef::Null();
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

    for (GBufferTarget& target : m_passes)
    {
        if (target.framebuffer.IsValid())
        {
            EnqueueDeletion(std::move(target.framebuffer));
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

    for (GBufferTarget& target : m_passes)
    {
        const GBufferPass pass = target.pass;

        switch (pass)
        {
        case GBufferPass::Opaque:
            target.framebuffer = CreateFramebuffer(nullptr, m_extent, pass);
            break;
        case GBufferPass::Lightmapped: // fallthrough
        case GBufferPass::Translucent: // fallthrough
        case GBufferPass::Effect:      // fallthrough
        case GBufferPass::Debug:       // fallthrough
            target.framebuffer = CreateFramebuffer(m_passes[uint8(GBufferPass::Opaque)].framebuffer, m_extent, pass);
            break;
        default:
            HYP_UNREACHABLE();
            break;
        }

        Assert(target.framebuffer.IsValid());
    }
}

FramebufferRef GBuffer::CreateFramebuffer(const FramebufferRef& parentFramebuffer, Vec2u resolution, GBufferPass pass)
{
    HYP_SCOPE;

    Assert(resolution.Volume() != 0);

    FramebufferDesc framebufferDesc;
    framebufferDesc.extent = resolution;
    framebufferDesc.numLayers = 1;

    FramebufferRef framebuffer = RI.MakeFramebuffer(framebufferDesc);

#if HYP_DEBUG_MODE
    framebuffer->SetDebugName(NAME_FMT("{}Framebuffer", EnumToString(pass)));
#endif

    auto addOwnedAttachment = [&](uint32 binding, TextureFormat format, LoadOperation loadOp = LoadOperation::CLEAR, StoreOperation storeOp = StoreOperation::STORE) -> Attachment*
    {
        return framebuffer->AddAttachment(
            binding,
            AttachmentDesc {
                TextureType::Texture2D,
                format,
                loadOp,
                storeOp });
    };

    auto addSharedAttachment = [&](uint32 binding, LoadOperation loadOp = LoadOperation::LOAD, StoreOperation storeOp = StoreOperation::STORE) -> Attachment*
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
    if (pass == GBufferPass::Opaque)
    {
        for (uint32 i = 0; i < GBufferTarget::Depth; i++)
        {
            const TextureFormat format = GetImageFormat(GBufferTarget::TargetName(i));

            addOwnedAttachment(i, format);
        }

        if (g_cvDepthPrepass.Get())
        {
            // If DepthPrepass is enabled, we don't CLEAR the depth texture as DPP is responsible for clearing it.
            addOwnedAttachment(GBufferTarget::Depth, GetImageFormat(GBufferTarget::Depth), LoadOperation::LOAD, StoreOperation::NONE);
        }
        else
        {
            // Otherwise, we clear it on render pass start.
            addOwnedAttachment(GBufferTarget::Depth, GetImageFormat(GBufferTarget::Depth), LoadOperation::CLEAR, StoreOperation::STORE);
        }
    }
    else
    {
        Assert(parentFramebuffer != nullptr);

        // add the attachments shared with opaque pass (including depth)
        for (uint8 i = 0; i < GBufferTarget::Max; i++)
        {
            switch (pass)
            {
            case GBufferPass::Effect:
                // EFFECT does not write normals, mat data, velocity, depth...
                // Use Store op == NONE for those
                if (i != GBufferTarget::Albedo)
                {
                    addOwnedAttachment(i, GetImageFormat(GBufferTarget::TargetName(i)), LoadOperation::LOAD, StoreOperation::NONE);

                    continue;
                }

                break;
            case GBufferPass::Debug:
                if (i == GBufferTarget::Depth)
                {
                    // debug bucket creates its own depth attachment
                    const TextureFormat format = GetImageFormat(GBufferTarget::TargetName(i));
                    addOwnedAttachment(i, format);

                    continue;
                }

                break;
            case GBufferPass::Lightmapped:
                if (i == GBufferTarget::Depth && g_cvDepthPrepass.Get())
                {
                    // Lightmapped objects are included in the depth prepass, so we don't want to write to depth when they render.
                    // Therefore we use StoreOperation::NONE as storeOp when DepthPrepass is true.
                    addSharedAttachment(i, LoadOperation::LOAD, StoreOperation::NONE);

                    continue;
                }

                break;
            default:
                break;
            }

            addSharedAttachment(i);
        }
    }

    CheckResult(framebuffer->Create());

    m_framebuffers.PushBack(framebuffer);

    return framebuffer;
}

#pragma endregion GBuffer

#pragma region GBufferTarget

Attachment* GBufferTarget::GetAttachment(TargetName resourceName) const
{
    HYP_SCOPE;

    Assert(framebuffer.IsValid());
    Assert(uint8(resourceName) < uint8(Max));

    return framebuffer->GetAttachment(uint32(resourceName));
}

#pragma endregion GBufferTarget

} // namespace Hyperion
