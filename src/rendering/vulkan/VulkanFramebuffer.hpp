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
            SafeDelete(std::move(it.second.attachment));
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
        RenderTargetType renderTargetType,
        LoadOperation loadOp,
        StoreOperation storeOp)
    {
        TextureDesc textureDesc;
        textureDesc.type = type;
        textureDesc.format = format;
        textureDesc.extent = Vec3u { extent.x, extent.y, 1 };
        textureDesc.imageUsage = IU_SAMPLED | IU_ATTACHMENT;

        VulkanGpuImageRef image = CreateObject<VulkanGpuImage>(textureDesc);

        VulkanAttachmentRef attachment = CreateObject<VulkanAttachment>(
            image,
            framebufferWeak,
            renderTargetType,
            loadOp,
            storeOp);

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
    VulkanFramebuffer(
        Vec2u extent,
        RenderTargetType renderTargetType = RTT_SHADER_RESOURCE,
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

    virtual VulkanAttachmentRef AddAttachment(const VulkanAttachmentRef& attachment) override;
    virtual VulkanAttachmentRef AddAttachment(uint32 binding, const VulkanGpuImageRef& image, LoadOperation loadOp, StoreOperation storeOp) override;

    virtual VulkanAttachmentRef AddAttachment(
        uint32 binding,
        TextureFormat format,
        TextureType type,
        LoadOperation loadOp,
        StoreOperation storeOp) override;

    virtual bool RemoveAttachment(uint32 binding) override;

    virtual VulkanAttachment* GetAttachment(uint32 binding) const override;

    virtual int NumAttachments() const override
    {
        return int(m_attachmentMap.Size());
    }

    HYP_FORCE_INLINE const VulkanAttachmentMap& GetAttachmentMap() const
    {
        return m_attachmentMap;
    }

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;

    virtual RendererResult Resize(Vec2u newSize) override;

    virtual void BeginCapture(VulkanCommandBuffer* commandBuffer) override;
    virtual void EndCapture(VulkanCommandBuffer* commandBuffer) override;

    virtual void Clear(VulkanCommandBuffer* commandBuffer) override;

private:
    VkFramebuffer m_handle;
    VulkanRenderPassRef m_renderPass;
    VulkanAttachmentMap m_attachmentMap;
};

} // namespace Hyperion
