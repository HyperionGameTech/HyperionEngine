/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Rendering/Attachment.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/Sampler.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class FramebufferBase : public ObjectBase
{
    HYP_OBJECT_BODY(FramebufferBase);

public:
    virtual ~FramebufferBase() override = default;

    static Pool* GetAllocator() { return g_rhiPool; }

    HYP_FORCE_INLINE uint32 GetWidth() const
    {
        return m_framebufferDesc.extent.x;
    }

    HYP_FORCE_INLINE uint32 GetHeight() const
    {
        return m_framebufferDesc.extent.y;
    }

    HYP_FORCE_INLINE const Vec2u& GetExtent() const
    {
        return m_framebufferDesc.extent;
    }

    HYP_FORCE_INLINE const FramebufferDesc& GetFramebufferDesc() const
    {
        return m_framebufferDesc;
    }

    virtual bool IsCreated() const = 0;

    /*! \brief Check if this framebuffer is currently capturing (deferred, see CommandRecorder).
     */
    HYP_FORCE_INLINE bool IsDeferredRecording() const
    {
        return m_isRecordingCount > 0;
    }

    /*! \brief Mark this framebuffer as being used for capturing via deferred commands (see CommandRecorder).
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

#if HYP_DEBUG_MODE
    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }
#endif

    virtual RendererResult Create() = 0;

    virtual Attachment* AddAttachment(Attachment* attachment) = 0;

    virtual Attachment* AddAttachment(uint32 binding, const AttachmentDesc& desc) = 0;
    virtual Attachment* AddAttachment(uint32 binding, const AttachmentDesc& desc, const GpuImageViewRef& imageView) = 0;

    virtual bool RemoveAttachment(uint32 binding) = 0;
    virtual Attachment* GetAttachment(uint32 binding) const = 0;
    virtual int NumAttachments() const = 0;

    virtual void BeginCapture(CommandBuffer* commandBuffer) = 0;
    virtual void EndCapture(CommandBuffer* commandBuffer) = 0;

    virtual void Clear(
        CommandBuffer* commandBuffer,
        uint8 attachmentsMask = uint8(-1)) = 0;

    virtual void Clear(
        CommandBuffer* commandBuffer,
        const Rect<uint32>& rect,
        uint8 attachmentsMask = uint8(-1)) = 0;

protected:
    FramebufferBase(const FramebufferDesc& framebufferDesc)
        : m_framebufferDesc(framebufferDesc),
          m_isRecordingCount(0)
    {
    }

    FramebufferDesc m_framebufferDesc;
    int m_isRecordingCount;

#if HYP_DEBUG_MODE
    Name m_debugName;
#endif
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <Rendering/Vulkan/VulkanFramebuffer.hpp>
#elif HYP_DX12
#include <Rendering/DX12/DX12Framebuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
