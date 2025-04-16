/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_SWAPCHAIN_HPP
#define HYPERION_BACKEND_RENDERER_SWAPCHAIN_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <core/math/Vector2.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

class SwapchainBase : public RenderObject<SwapchainBase>
{
public:
    virtual ~SwapchainBase() override = default;

    virtual bool IsCreated() const = 0;

    HYP_FORCE_INLINE const Array<ImageRef> &GetImages() const
        { return m_images; }

    HYP_FORCE_INLINE Vec2u GetExtent() const
        { return m_extent; }

    HYP_FORCE_INLINE InternalFormat GetImageFormat() const
        { return m_image_format; }

    HYP_FORCE_INLINE uint32 GetAcquiredImageIndex() const
        { return m_acquired_image_index; }

    HYP_FORCE_INLINE uint32 GetCurrentFrameIndex() const
        { return m_current_frame_index; }

protected:
    SwapchainBase()
        : m_extent(Vec2i::Zero()),
          m_acquired_image_index(0),
          m_current_frame_index(0)
    {
    }

    Array<ImageRef> m_images;
    Vec2u           m_extent;
    InternalFormat  m_image_format = InternalFormat::NONE;
    uint32          m_acquired_image_index;
    uint32          m_current_frame_index;
};

} // namespace renderer
} // namespace hyperion

#endif