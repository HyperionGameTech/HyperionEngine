/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>
#include <rendering/RenderGpuImage.hpp>

#include <core/functional/Proc.hpp>

#include <core/math/Vector2.hpp>

#include <core/Defines.hpp>

namespace hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class SwapchainBase : public ObjectBase
{
    HYP_OBJECT_BODY(SwapchainBase);

public:
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

    HYP_FORCE_INLINE bool IsPqHdr() const
    {
        return m_isPqHdr;
    }

    HYP_FORCE_INLINE uint32 GetAcquiredImageIndex() const
    {
        return m_acquiredImageIndex;
    }

    virtual RendererResult Create() = 0;
    virtual SwapchainRef Recreate() = 0;

protected:
    explicit SwapchainBase(const Vec2u& extent = Vec2u::Zero())
        : m_extent(extent),
          m_acquiredImageIndex(0),
          m_imageFormat(TF_NONE),
          m_isPqHdr(false)
    {
    }

    Array<GpuImageRef> m_images;
    Array<FramebufferRef> m_framebuffers;
    Vec2u m_extent;
    TextureFormat m_imageFormat;
    uint32 m_acquiredImageIndex;
    bool m_isPqHdr : 1;
};

} // namespace hyperion
