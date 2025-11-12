/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderAttachment.hpp>
#include <rendering/RenderGpuImage.hpp>
#include <rendering/RenderGpuImageView.hpp>
#include <rendering/RenderSampler.hpp>

#include <core/Defines.hpp>

namespace hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class FramebufferBase : public ObjectBase
{
    HYP_OBJECT_BODY(FramebufferBase);

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

    virtual bool IsCreated() const = 0;

    /*! \brief Check if this framebuffer is currently capturing (deferred, see RenderQueue).
     */
    HYP_FORCE_INLINE bool IsDeferredRecording() const
    {
        return m_isRecordingCount > 0;
    }

    /*! \brief Mark this framebuffer as being used for capturing via deferred commands (see RenderQueue).
     *  Since commands are deferred and dont actually build GPU command buffers until the end of the frame, we need to track whether or not we're recording
     *  to avoid nested framebuffer begin/end calls.
     */
    HYP_FORCE_INLINE void SetIsDeferredRecording(bool value)
    {
        if (value)
        {
            m_isRecordingCount++;
        }
        else
        {
            HYP_CORE_ASSERT(m_isRecordingCount > 0);
            m_isRecordingCount--;
        }
    }

    virtual RendererResult Create() = 0;

    virtual RendererResult Resize(Vec2u newSize) = 0;

    virtual AttachmentRef AddAttachment(const AttachmentRef& attachment) = 0;
    virtual AttachmentRef AddAttachment(uint32 binding, const GpuImageRef& image, LoadOperation loadOp, StoreOperation storeOp) = 0;
    virtual AttachmentRef AddAttachment(
        uint32 binding,
        TextureFormat format,
        TextureType type,
        LoadOperation loadOp,
        StoreOperation storeOp) = 0;

    virtual bool RemoveAttachment(uint32 binding) = 0;
    virtual AttachmentBase* GetAttachment(uint32 binding) const = 0;
    virtual int NumAttachments() const = 0;

    virtual void BeginCapture(CommandBufferBase* commandBuffer) = 0;
    virtual void EndCapture(CommandBufferBase* commandBuffer) = 0;

    virtual void Clear(CommandBufferBase* commandBuffer) = 0;

protected:
    FramebufferBase(Vec2u extent)
        : m_extent(extent),
          m_isRecordingCount(0)
    {
    }

    Vec2u m_extent;
    int m_isRecordingCount;
};

} // namespace hyperion
