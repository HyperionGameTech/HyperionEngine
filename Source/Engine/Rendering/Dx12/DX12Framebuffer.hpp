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
#include <Rendering/DX12/DX12Attachment.hpp>
#include <Rendering/DX12/DX12DescriptorHeaps.hpp>

#include <Core/Containers/FlatMap.hpp>

namespace Hyperion {

enum class RenderPassMode : uint8;

struct DX12AttachmentMap
{
    using Iterator = typename FlatMap<uint32, DX12Attachment*>::Iterator;
    using ConstIterator = typename FlatMap<uint32, DX12Attachment*>::ConstIterator;

    DX12FramebufferWeakRef framebufferWeak;
    FlatMap<uint32, DX12Attachment*> attachments;

    ~DX12AttachmentMap()
    {
        Reset();
    }

    RendererResult Create();

    void Reset()
    {
        for (auto& it : attachments)
        {
            DX12Attachment* attachment = it.second;
            if (!attachment)
                continue;

            attachment->Release();
        }

        attachments.Clear();
    }

    HYP_FORCE_INLINE size_t Size() const
    {
        return attachments.Size();
    }

    DX12Attachment* GetAttachment(uint32 binding) const
    {
        const auto it = attachments.Find(binding);

        if (it == attachments.End())
        {
            return nullptr;
        }

        return it->second;
    }

    DX12Attachment* AddAttachment(DX12Attachment* attachment)
    {
        Assert(attachment != nullptr);
        Assert(attachment->GetGpuImage() != nullptr);

        Assert(attachment->HasBinding(), "Attachment must have a binding");

        const uint32 binding = attachment->GetBinding();
        Assert(!attachments.Contains(binding), "Attachment already exists at binding: {}", binding);

        attachments[binding] = attachment;

        return attachment;
    }

    DX12Attachment* AddAttachment(
        uint32 binding,
        Vec2u extent,
        const AttachmentDesc& attachmentDesc,
        RenderPassMode renderPassMode)
    {
        TextureDesc textureDesc {};
        textureDesc.type = attachmentDesc.imageType;
        textureDesc.format = attachmentDesc.format;
        textureDesc.extent = Vec3u { extent.x, extent.y, 1 };
        textureDesc.wrapMode = TextureWrapMode::TWM_CLAMP_TO_EDGE;
        textureDesc.imageUsage = IU_SAMPLED | IU_ATTACHMENT;

        DX12Attachment* attachment = new DX12Attachment(
            textureDesc,
            framebufferWeak,
            renderPassMode,
            attachmentDesc);

        attachment->SetBinding(binding);

        attachments[binding] = attachment;

        return attachment;
    }

    HYP_DEF_STL_BEGIN_END(attachments.Begin(), attachments.End())
};

HYP_CLASS(NoScriptBindings)
class DX12Framebuffer final : public FramebufferBase
{
    HYP_OBJECT_BODY(DX12Framebuffer);

public:
    explicit DX12Framebuffer(const FramebufferDesc& framebufferDesc);
    ~DX12Framebuffer() override;

    HYP_FORCE_INLINE const DX12AttachmentMap& GetAttachmentMap() const
    {
        return m_attachmentMap;
    }

#if HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

    RendererResult Create() override;

    void SetExternalRTVHandle(
        const D3D12_CPU_DESCRIPTOR_HANDLE& handle,
        ID3D12Resource* resource,
        const Vec2u& extent,
        TextureFormat format)
    {
        m_rtvDescriptorHandle.cpuHandle = handle;
        m_rtvDescriptorHandle.count = 1;
        m_isExternalRTV = true;
        m_externalRTResource = resource;

        // New swapchain back buffer starts in PRESENT state after Present()
        m_externalRTResourceState = D3D12_RESOURCE_STATE_PRESENT;

        // Populate framebufferDesc so the graphics pipeline sees at least 1 attachment
        m_framebufferDesc.numAttachments = 1;
        m_framebufferDesc.extent = extent;
        m_framebufferDesc.attachments[0] = AttachmentDesc(TextureType::Texture2D, format);
    }

    DX12Attachment* AddAttachment(DX12Attachment* attachment) override;

    DX12Attachment* AddAttachment(uint32 binding, const AttachmentDesc& desc) override;
    DX12Attachment* AddAttachment(uint32 binding, const AttachmentDesc& desc, const DX12GpuImageViewRef& imageView) override;

    bool RemoveAttachment(uint32 binding) override;

    DX12Attachment* GetAttachment(uint32 binding) const override;

    int NumAttachments() const override
    {
        return int(m_attachmentMap.Size());
    }

    bool IsCreated() const override
    {
        return m_isCreated;
    }

    void BeginCapture(DX12CommandBuffer* commandBuffer) override;
    void EndCapture(DX12CommandBuffer* commandBuffer) override;

    void ResetExternalRTResourceState()
    {
        if (m_externalRTResource != nullptr)
        {
            m_externalRTResourceState = D3D12_RESOURCE_STATE_PRESENT;
        }
    }

    void Clear(
        DX12CommandBuffer* commandBuffer,
        uint8 attachmentsMask = uint8(-1)) override;

    void Clear(
        DX12CommandBuffer* commandBuffer,
        const Rect<uint32>& rect,
        uint8 attachmentsMask = uint8(-1)) override;

private:
    DX12AttachmentMap m_attachmentMap;

    DX12DescriptorHandle m_rtvDescriptorHandle;
    DX12DescriptorHandle m_dsvDescriptorHandle;

    bool m_isRecording;
    bool m_isCreated;
    bool m_hasBeenCleared;
    bool m_isExternalRTV = false;
    ID3D12Resource* m_externalRTResource = nullptr;
    D3D12_RESOURCE_STATES m_externalRTResourceState = D3D12_RESOURCE_STATE_COMMON;
};

} // namespace Hyperion
