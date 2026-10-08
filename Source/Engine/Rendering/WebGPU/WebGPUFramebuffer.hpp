/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/Framebuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/RenderTypes.hpp>

#include <Rendering/WebGPU/WebGPUShared.hpp>
#include <Rendering/WebGPU/WebGPUAttachment.hpp>

#include <Core/Containers/FlatMap.hpp>

namespace Hyperion {

enum class RenderPassMode : uint8;

HYP_CLASS(NoScriptBindings)
class WebGPUFramebuffer final : public FramebufferBase
{
    HYP_OBJECT_BODY(WebGPUFramebuffer);

public:
    explicit WebGPUFramebuffer(const FramebufferDesc& framebufferDesc);
    ~WebGPUFramebuffer() override;

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override;
#endif

    RendererResult Create() override;

    void SetExternalTextureView(WGPUTextureView textureView, const Vec2u& extent, TextureFormat format);

    WebGPUAttachment* AddAttachment(WebGPUAttachment* attachment) override;
    WebGPUAttachment* AddAttachment(uint32 binding, const AttachmentDesc& desc) override;
    WebGPUAttachment* AddAttachment(uint32 binding, const AttachmentDesc& desc, const WebGPUGpuImageViewRef& imageView) override;

    bool RemoveAttachment(uint32 binding) override;
    WebGPUAttachment* GetAttachment(uint32 binding) const override;

    int NumAttachments() const override
    {
        return int(m_attachments.Size());
    }

    bool IsCreated() const override
    {
        return m_isCreated;
    }

    void BeginCapture(WebGPUCommandBuffer* commandBuffer) override;
    void EndCapture(WebGPUCommandBuffer* commandBuffer) override;

    void Clear(
        WebGPUCommandBuffer* commandBuffer,
        uint8 attachmentsMask = uint8(-1)) override;

    void Clear(
        WebGPUCommandBuffer* commandBuffer,
        const Rect<uint32>& rect,
        uint8 attachmentsMask = uint8(-1)) override;

    // Size of what is actually rendered into. Shadow maps render into part of a larger atlas image, so this can
    // exceed the extent the framebuffer was described with.
    Vec2u GetAttachmentExtent() const;

    HYP_FORCE_INLINE bool HasPendingClears() const
    {
        return m_pendingClearMask != 0;
    }

    // clears are load operations in WebGPU, so the ones requested since the pass last closed are applied here
    WGPURenderPassEncoder BeginPass(WGPUCommandEncoder encoder);

    static void ReleaseSharedResources();

private:
    uint32 GetAllAttachmentsMask() const;

    FlatMap<uint32, WebGPUAttachment*> m_attachments;

    WGPUTextureView m_externalTextureView;

    uint32 m_pendingClearMask;
    bool m_isRecording;
    bool m_isCreated;
};

} // namespace Hyperion
