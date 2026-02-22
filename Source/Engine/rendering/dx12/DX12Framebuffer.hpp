/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/Framebuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/RenderObject.hpp>
#include <rendering/dx12/DX12Attachment.hpp>
#include <rendering/dx12/DX12DescriptorHeaps.hpp>

#include <Core/containers/FlatMap.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12Framebuffer final : public FramebufferBase
{
    HYP_OBJECT_BODY(DX12Framebuffer);

public:
    explicit DX12Framebuffer(const RenderTargetDesc& renderTargetDesc);
    ~DX12Framebuffer() override;

    bool IsCreated() const override;
    RendererResult Create() override;

    DX12Attachment* AddAttachment(DX12Attachment* attachment) override;
    DX12Attachment* AddAttachment(uint32 binding, const DX12GpuImageRef& image, LoadOperation loadOp, StoreOperation storeOp) override;

    DX12Attachment* AddAttachment(
        uint32 binding,
        TextureFormat format,
        TextureType type,
        LoadOperation loadOp,
        StoreOperation storeOp) override;

    bool RemoveAttachment(uint32 binding) override;
    DX12Attachment* GetAttachment(uint32 binding) const override;
    int NumAttachments() const override;

    HYP_FORCE_INLINE const FlatMap<uint32, DX12Attachment*>& GetAttachments() const
    {
        return m_attachments;
    }

    void BeginCapture(DX12CommandBuffer* commandBuffer) override;
    void EndCapture(DX12CommandBuffer* commandBuffer) override;

    void Clear(DX12CommandBuffer* commandBuffer) override;

private:
    bool m_isCreated;

    FlatMap<uint32, DX12Attachment*> m_attachments;

    DX12DescriptorHandle m_rtvDescriptorHandle;
    DX12DescriptorHandle m_dsvDescriptorHandle;
};

} // namespace Hyperion
