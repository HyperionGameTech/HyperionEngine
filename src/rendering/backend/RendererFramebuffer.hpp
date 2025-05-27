/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_FBO_HPP
#define HYPERION_BACKEND_RENDERER_FBO_HPP

#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/Platform.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

class FramebufferBase : public RenderObject<FramebufferBase>
{
public:
    virtual ~FramebufferBase() override = default;

    HYP_FORCE_INLINE uint32 GetWidth() const
    {
        return m_extent.x;
    }

    HYP_FORCE_INLINE uint32 GetHeight() const
    {
        return m_extent.y;
    }

    HYP_FORCE_INLINE const Vec2u& GetExtent() const
    {
        return m_extent;
    }

    HYP_API virtual bool IsCreated() const = 0;

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

    HYP_API virtual RendererResult Resize(Vec2u new_size) = 0;

    HYP_API virtual AttachmentRef AddAttachment(const AttachmentRef& attachment) = 0;
    HYP_API virtual AttachmentRef AddAttachment(uint32 binding, const ImageRef& image, LoadOperation load_op, StoreOperation store_op) = 0;
    HYP_API virtual AttachmentRef AddAttachment(
        uint32 binding,
        InternalFormat format,
        ImageType type,
        LoadOperation load_op,
        StoreOperation store_op) = 0;

    HYP_API virtual bool RemoveAttachment(uint32 binding) = 0;
    HYP_API virtual AttachmentBase* GetAttachment(uint32 binding) const = 0;

    HYP_API virtual void BeginCapture(CommandBufferBase* command_buffer, uint32 frame_index) = 0;
    HYP_API virtual void EndCapture(CommandBufferBase* command_buffer, uint32 frame_index) = 0;

protected:
    FramebufferBase(Vec2u extent)
        : m_extent(extent)
    {
    }

    Vec2u m_extent;
};

} // namespace renderer
} // namespace hyperion

#endif