/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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

#include <rendering/util/SafeDeleter.hpp>

#include <core/containers/FlatMap.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

class VulkanCommandBuffer;

enum class VulkanRenderPassMode : uint8;

extern Pool* g_vulkanPool;

struct VulkanAttachmentDef
{
    VulkanGpuImageRef image;
    VulkanAttachment* attachment = nullptr;
};

struct VulkanAttachmentMap
{
    using Iterator = typename FlatMap<uint32, VulkanAttachmentDef>::Iterator;
    using ConstIterator = typename FlatMap<uint32, VulkanAttachmentDef>::ConstIterator;

    VulkanFramebufferWeakRef framebufferWeak;
    FlatMap<uint32, VulkanAttachmentDef> attachments;

    ~VulkanAttachmentMap()
    {
        Reset();
    }

    RendererResult Create();

    void Reset()
    {
        for (auto& it : attachments)
        {
            Attachment* attachment = it.second.attachment;
            if (!attachment)
                continue;

            attachment->Release();
        }

        attachments.Clear();
    }

    HYP_FORCE_INLINE SizeType Size() const
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

        return it->second.attachment;
    }

    VulkanAttachment* AddAttachment(VulkanAttachment* attachment)
    {
        Assert(attachment != nullptr);
        Assert(attachment->GetImage().IsValid());

        Assert(attachment->HasBinding(), "Attachment must have a binding");

        const uint32 binding = attachment->GetBinding();
        Assert(!attachments.Contains(binding), "Attachment already exists at binding: {}", binding);

        attachments.Set(
            binding,
            VulkanAttachmentDef {
                VulkanGpuImageRef(attachment->GetImage()),
                attachment });

        return attachment;
    }

    VulkanAttachment* AddAttachment(
        uint32 binding,
        Vec2u extent,
        TextureFormat format,
        TextureType type,
        VulkanRenderPassMode renderPassMode,
        LoadOperation loadOp,
        StoreOperation storeOp)
    {
        TextureDesc textureDesc;
        textureDesc.type = type;
        textureDesc.format = format;
        textureDesc.extent = Vec3u { extent.x, extent.y, 1 };
        textureDesc.imageUsage = IU_SAMPLED | IU_ATTACHMENT;

        VulkanGpuImageRef image = MakeHandle<VulkanGpuImage>(textureDesc);

        VulkanAttachment* attachment = new VulkanAttachment(
            image,
            framebufferWeak,
            renderPassMode,
            AttachmentDesc {
                .imageType = type,
                .format = format,
                .loadOp = loadOp,
                .storeOp = storeOp,
                .blendFunction = BlendFunction::None(),
                .clearColor = {} });

        attachment->SetBinding(binding);

        attachments.Set(
            binding,
            VulkanAttachmentDef {
                image,
                attachment });

        return attachment;
    }

    HYP_DEF_STL_BEGIN_END(attachments.Begin(), attachments.End())
};

HYP_CLASS(NoScriptBindings)
class VulkanFramebuffer final : public FramebufferBase
{
    HYP_OBJECT_BODY(VulkanFramebuffer);

public:
    VulkanFramebuffer(const RenderTargetDesc& renderTargetDesc, VulkanRenderPassMode renderPassMode);
    ~VulkanFramebuffer() override;

    HYP_FORCE_INLINE const VkFramebuffer& GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE const VulkanRenderPass& GetRenderPass() const
    {
        return m_renderPass;
    }

    HYP_FORCE_INLINE VulkanRenderPassMode GetRenderPassMode() const
    {
        return m_renderPassMode;
    }

    HYP_FORCE_INLINE const VulkanAttachmentMap& GetAttachmentMap() const
    {
        return m_attachmentMap;
    }

    VulkanAttachment* AddAttachment(VulkanAttachment* attachment) override;
    VulkanAttachment* AddAttachment(uint32 binding, const VulkanGpuImageRef& image, LoadOperation loadOp, StoreOperation storeOp) override;

    VulkanAttachment* AddAttachment(
        uint32 binding,
        TextureFormat format,
        TextureType type,
        LoadOperation loadOp,
        StoreOperation storeOp) override;

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

    void Clear(VulkanCommandBuffer* commandBuffer) override;

private:
    VkFramebuffer m_handle;
    VulkanRenderPassMode m_renderPassMode;
    VulkanRenderPass m_renderPass;
    VulkanAttachmentMap m_attachmentMap;
};

} // namespace Hyperion
