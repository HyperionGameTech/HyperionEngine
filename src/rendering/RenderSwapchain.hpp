/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>
#include <rendering/RenderGpuImage.hpp>

#include <core/functional/Proc.hpp>

#include <core/math/Vector2.hpp>

#include <core/Defines.hpp>

namespace hyperion {

class SwapchainBase : public RenderObject<SwapchainBase>
{
public:
    virtual ~SwapchainBase() override = default;

    virtual bool IsCreated() const = 0;

    HYP_FORCE_INLINE const Array<GpuImageRef>& GetImages() const
    {
        return m_images;
    }

    HYP_FORCE_INLINE const Array<FramebufferRef>& GetFramebuffers() const
    {
        return m_framebuffers;
    }

    HYP_FORCE_INLINE Vec2u GetExtent() const
    {
        return m_extent;
    }

    HYP_FORCE_INLINE TextureFormat GetImageFormat() const
    {
        return m_imageFormat;
    }

    HYP_FORCE_INLINE uint32 GetAcquiredImageIndex() const
    {
        return m_acquiredImageIndex;
    }

    HYP_FORCE_INLINE uint32 GetCurrentFrameIndex() const
    {
        return m_currentFrameIndex;
    }

    virtual RendererResult Create() = 0;
    virtual RendererResult Destroy() = 0;

protected:
    SwapchainBase()
        : m_extent(Vec2i::Zero()),
          m_acquiredImageIndex(0),
          m_currentFrameIndex(0)
    {
    }

    Array<GpuImageRef> m_images;
    Array<FramebufferRef> m_framebuffers;
    Vec2u m_extent;
    TextureFormat m_imageFormat = TF_NONE;
    uint32 m_acquiredImageIndex;
    uint32 m_currentFrameIndex;
};

} // namespace hyperion
