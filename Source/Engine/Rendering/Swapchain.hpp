/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/GpuImage.hpp>

#include <Core/Functional/Proc.hpp>
#include <Core/Functional/Delegate.hpp>

#include <Core/Math/Vector2.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class SwapchainBase : public ObjectBase
{
    HYP_OBJECT_BODY(SwapchainBase);

public:
    static Pool* GetAllocator() { return g_rhiPool; }

    virtual ~SwapchainBase() override = default;

    virtual bool IsCreated() const = 0;

    HYP_FORCE_INLINE const Array<GpuImageRef>& GetImages() const
    {
        return m_images;
    }

    HYP_FORCE_INLINE uint32 NumAcquiredImages() const
    {
        return uint32(m_images.Size());
    }

    HYP_FORCE_INLINE const Array<FramebufferRef>& GetFramebuffers() const
    {
        return m_framebuffers;
    }

    HYP_FORCE_INLINE const Vec2u& GetExtent() const
    {
        return m_extent;
    }

    HYP_FORCE_INLINE TextureFormat GetImageFormat() const
    {
        return m_imageFormat;
    }

    HYP_FORCE_INLINE bool NeedsRecreate() const
    {
        return m_needsRecreate;
    }

    HYP_FORCE_INLINE bool IsPqHdr() const
    {
        return m_isPqHdr;
    }

    HYP_FORCE_INLINE uint32 GetAcquiredImageIndex() const
    {
        return m_acquiredImageIndex;
    }

    virtual RendererResult Create() = 0;
    virtual void SetExtent(Vec2u newExtent) = 0;
    virtual void Recreate() = 0;

protected:
    explicit SwapchainBase(const Vec2u& extent = Vec2u::Zero())
        : m_extent(extent),
          m_acquiredImageIndex(0),
          m_imageFormat(InvalidTextureFormat),
          m_needsRecreate(false),
          m_isPqHdr(false)
    {
    }

    Array<GpuImageRef> m_images;
    Array<FramebufferRef> m_framebuffers;
    Vec2u m_extent;
    TextureFormat m_imageFormat;
    uint32 m_acquiredImageIndex;
    bool m_needsRecreate;
    bool m_isPqHdr;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <Rendering/Vulkan/VulkanSwapchain.hpp>
#elif HYP_DX12
#include <Rendering/DX12/DX12Swapchain.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
