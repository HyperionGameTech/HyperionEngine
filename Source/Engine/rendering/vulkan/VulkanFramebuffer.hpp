/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/Framebuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/vulkan/VulkanAttachment.hpp>
#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanRenderPass.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/containers/FlatMap.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

class VulkanCommandBuffer;

enum class RenderPassMode : uint8;

extern Pool* g_vulkanPool;

struct VulkanAttachmentMap
{
    using Iterator = typename FlatMap<uint32, VulkanAttachment*>::Iterator;
    using ConstIterator = typename FlatMap<uint32, VulkanAttachment*>::ConstIterator;

    VulkanFramebufferWeakRef framebufferWeak;
    FlatMap<uint32, VulkanAttachment*> attachments;

    ~VulkanAttachmentMap()
    {
        Reset();
    }

    RendererResult Create();

    void Reset()
    {
        for (auto& it : attachments)
        {
            VulkanAttachment* attachment = it.second;
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

    VulkanAttachment* GetAttachment(uint32 binding) const
    {
        const auto it = attachments.Find(binding);

        if (it == attachments.End())
        {
            return nullptr;
        }

        return it->second;
    }

    VulkanAttachment* AddAttachment(VulkanAttachment* attachment)
    {
        Assert(attachment != nullptr);
        Assert(attachment->GetGpuImage() != nullptr);

        Assert(attachment->HasBinding(), "Attachment must have a binding");

        const uint32 binding = attachment->GetBinding();
        Assert(!attachments.Contains(binding), "Attachment already exists at binding: {}", binding);

        attachments[binding] = attachment;

        return attachment;
    }

    VulkanAttachment* AddAttachment(
        uint32 binding,
        Vec2u extent,
        const AttachmentDesc& attachmentDesc,
        RenderPassMode renderPassMode)
    {
        TextureDesc textureDesc {};
        textureDesc.type = attachmentDesc.imageType;
        textureDesc.format = attachmentDesc.format;
        textureDesc.extent = Vec3u { extent.x, extent.y, 1 };
        textureDesc.imageUsage = IU_SAMPLED | IU_ATTACHMENT;

        VulkanAttachment* attachment = new VulkanAttachment(
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
class VulkanFramebuffer final : public FramebufferBase
{
    HYP_OBJECT_BODY(VulkanFramebuffer);

public:
    explicit VulkanFramebuffer(const RenderTargetDesc& renderTargetDesc);
    ~VulkanFramebuffer() override;

    HYP_FORCE_INLINE const VkFramebuffer& GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE const VulkanRenderPass& GetRenderPass() const
    {
        return m_renderPass;
    }

    HYP_FORCE_INLINE const VulkanAttachmentMap& GetAttachmentMap() const
    {
        return m_attachmentMap;
    }

#if HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

    VulkanAttachment* AddAttachment(VulkanAttachment* attachment) override;

    VulkanAttachment* AddAttachment(uint32 binding, const AttachmentDesc& desc) override;
    VulkanAttachment* AddAttachment(uint32 binding, const AttachmentDesc& desc, const VulkanGpuImageViewRef& imageView) override;

    bool RemoveAttachment(uint32 binding) override;

    VulkanAttachment* GetAttachment(uint32 binding) const override;

    int NumAttachments() const override
    {
        return int(m_attachmentMap.Size());
    }
    
    bool IsCreated() const override;

    RendererResult Create() override;

    void BeginCapture(VulkanCommandBuffer* commandBuffer) override;
    void EndCapture(VulkanCommandBuffer* commandBuffer) override;

    void Clear(
        VulkanCommandBuffer* commandBuffer,
        uint8 attachmentsMask = uint8(-1)) override;

    void Clear(
        VulkanCommandBuffer* commandBuffer,
        const Rect<uint32>& rect,
        uint8 attachmentsMask = uint8(-1)) override;

private:
    VkFramebuffer m_handle;
    VulkanRenderPass m_renderPass;
    VulkanAttachmentMap m_attachmentMap;
};

} // namespace Hyperion
