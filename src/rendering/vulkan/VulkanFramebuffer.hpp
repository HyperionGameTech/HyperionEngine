/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderFramebuffer.hpp>

#include <rendering/vulkan/VulkanAttachment.hpp>
#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanRenderPass.hpp>

#include <core/containers/FlatMap.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanCommandBuffer;

struct VulkanAttachmentDef
{
    VulkanGpuImageRef image;
    VulkanAttachmentRef attachment;
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
    RendererResult Resize(Vec2u newSize);

    void Reset()
    {
        for (auto& it : attachments)
        {
            SafeRelease(std::move(it.second.attachment));
        }

        attachments.Clear();
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        return attachments.Size();
    }

    HYP_FORCE_INLINE const VulkanAttachmentRef& GetAttachment(uint32 binding) const
    {
        const auto it = attachments.Find(binding);

        if (it == attachments.End())
        {
            return VulkanAttachmentRef::Null();
        }

        return it->second.attachment;
    }

    HYP_FORCE_INLINE VulkanAttachmentRef AddAttachment(const VulkanAttachmentRef& attachment)
    {
        Assert(attachment.IsValid());
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

    HYP_FORCE_INLINE VulkanAttachmentRef AddAttachment(
        uint32 binding,
        Vec2u extent,
        TextureFormat format,
        TextureType type,
        RenderPassStage stage,
        LoadOperation loadOp,
        StoreOperation storeOp)
    {
        TextureDesc textureDesc;
        textureDesc.type = type;
        textureDesc.format = format;
        textureDesc.extent = Vec3u { extent.x, extent.y, 1 };
        textureDesc.imageUsage = IU_SAMPLED | IU_ATTACHMENT;

        VulkanGpuImageRef image = MakeRenderObject<VulkanGpuImage>(textureDesc);

        VulkanAttachmentRef attachment = MakeRenderObject<VulkanAttachment>(image, framebufferWeak, stage, loadOp, storeOp);
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

class VulkanFramebuffer final : public FramebufferBase
{
public:
    VulkanFramebuffer(
        Vec2u extent,
        RenderPassStage stage = RenderPassStage::SHADER,
        uint32 numMultiviewLayers = 0);

    virtual ~VulkanFramebuffer() override;

    HYP_FORCE_INLINE const VkFramebuffer& GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE const VulkanRenderPassRef& GetRenderPass() const
    {
        return m_renderPass;
    }

    virtual AttachmentRef AddAttachment(const AttachmentRef& attachment) override;
    virtual AttachmentRef AddAttachment(uint32 binding, const GpuImageRef& image, LoadOperation loadOp, StoreOperation storeOp) override;

    virtual AttachmentRef AddAttachment(
        uint32 binding,
        TextureFormat format,
        TextureType type,
        LoadOperation loadOp,
        StoreOperation storeOp) override;

    virtual bool RemoveAttachment(uint32 binding) override;

    virtual AttachmentBase* GetAttachment(uint32 binding) const override;

    HYP_FORCE_INLINE const VulkanAttachmentMap& GetAttachmentMap() const
    {
        return m_attachmentMap;
    }

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;
    virtual RendererResult Destroy() override;

    virtual RendererResult Resize(Vec2u newSize) override;

    virtual void BeginCapture(CommandBufferBase* commandBuffer) override;
    virtual void EndCapture(CommandBufferBase* commandBuffer) override;

    virtual void Clear(CommandBufferBase* commandBuffer) override;

private:
    VkFramebuffer m_handle;
    VulkanRenderPassRef m_renderPass;
    VulkanAttachmentMap m_attachmentMap;
};

} // namespace hyperion
