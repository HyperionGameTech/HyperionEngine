/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

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
    
    DX12Attachment* AddAttachment(uint32 binding, const AttachmentDesc& desc) = 0;
    DX12Attachment* AddAttachment(uint32 binding, const AttachmentDesc& desc, const DX12GpuImageViewRef& imageView) = 0;

    bool RemoveAttachment(uint32 binding) override;
    DX12Attachment* GetAttachment(uint32 binding) const override;
    int NumAttachments() const override;

    HYP_FORCE_INLINE const FlatMap<uint32, DX12Attachment*>& GetAttachments() const
    {
        return m_attachments;
    }

    void BeginCapture(DX12CommandBuffer* commandBuffer) override;
    void EndCapture(DX12CommandBuffer* commandBuffer) override;

    void Clear(DX12CommandBuffer* commandBuffer, uint8 attachmentsMask = uint8(-1)) override;

private:
    bool m_isCreated;

    FlatMap<uint32, DX12Attachment*> m_attachments;

    DX12DescriptorHandle m_rtvDescriptorHandle;
    DX12DescriptorHandle m_dsvDescriptorHandle;
};

} // namespace Hyperion
