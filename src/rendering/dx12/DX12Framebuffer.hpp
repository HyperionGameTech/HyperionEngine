/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/Framebuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/RenderObject.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12Framebuffer final : public FramebufferBase
{
    HYP_OBJECT_BODY(DX12Framebuffer);

public:
    DX12Framebuffer(Vec2u extent, RenderTargetType renderTargetType, uint32 numViews = 1);
    ~DX12Framebuffer() override;

    bool IsCreated() const override;
    RendererResult Create() override;
    RendererResult Resize(Vec2u newSize) override;

    AttachmentRef AddAttachment(const AttachmentRef& attachment) override;
    AttachmentRef AddAttachment(uint32 binding, const GpuImageRef& image, LoadOperation loadOp, StoreOperation storeOp) override;
    AttachmentRef AddAttachment(
        uint32 binding,
        TextureFormat format,
        TextureType type,
        LoadOperation loadOp,
        StoreOperation storeOp) override;

    bool RemoveAttachment(uint32 binding) override;
    AttachmentBase* GetAttachment(uint32 binding) const override;
    int NumAttachments() const override;

    void BeginCapture(CommandBuffer* commandBuffer) override;
    void EndCapture(CommandBuffer* commandBuffer) override;

    void Clear(CommandBuffer* commandBuffer) override;
};

} // namespace Hyperion
