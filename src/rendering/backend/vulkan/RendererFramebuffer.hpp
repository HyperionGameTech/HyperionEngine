/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_FBO_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_FBO_HPP

#include <rendering/backend/RendererFramebuffer.hpp>

#include <rendering/backend/vulkan/RendererAttachment.hpp>
#include <rendering/backend/vulkan/RendererImage.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/RendererRenderPass.hpp>

#include <core/containers/FlatMap.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class VulkanCommandBuffer;

struct VulkanAttachmentDef
{
    VulkanImageRef image;
    VulkanAttachmentRef attachment;
};

struct VulkanAttachmentMap
{
    using Iterator = typename FlatMap<uint32, VulkanAttachmentDef>::Iterator;
    using ConstIterator = typename FlatMap<uint32, VulkanAttachmentDef>::ConstIterator;

    VulkanFramebufferWeakRef framebuffer;
    FlatMap<uint32, VulkanAttachmentDef> attachments;

    ~VulkanAttachmentMap()
    {
        Reset();
    }

    RendererResult Create();
    RendererResult Resize(Vec2u new_size);

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
        AssertThrow(attachment.IsValid());
        AssertThrow(attachment->GetImage().IsValid());

        AssertThrowMsg(attachment->HasBinding(), "Attachment must have a binding");

        const uint32 binding = attachment->GetBinding();
        AssertThrowMsg(!attachments.Contains(binding), "Attachment already exists at binding: %u", binding);

        attachments.Set(
            binding,
            VulkanAttachmentDef {
                VulkanImageRef(attachment->GetImage()),
                attachment });

        return attachment;
    }

    HYP_FORCE_INLINE VulkanAttachmentRef AddAttachment(
        uint32 binding,
        Vec2u extent,
        InternalFormat format,
        ImageType type,
        RenderPassStage stage,
        LoadOperation load_op,
        StoreOperation store_op)
    {
        TextureDesc texture_desc;
        texture_desc.type = type;
        texture_desc.format = format;
        texture_desc.extent = Vec3u { extent.x, extent.y, 1 };
        texture_desc.image_format_capabilities = ImageFormatCapabilities::SAMPLED | ImageFormatCapabilities::ATTACHMENT;

        VulkanImageRef image = MakeRenderObject<VulkanImage>(texture_desc);

        VulkanAttachmentRef attachment = MakeRenderObject<VulkanAttachment>(image, framebuffer, stage, load_op, store_op);
        attachment->SetBinding(binding);

        attachments.Set(
            binding,
            VulkanAttachmentDef {
                image,
                attachment });

        return attachment;
    }

    HYP_DEF_STL_BEGIN_END(
        attachments.Begin(),
        attachments.End())
};

class VulkanFramebuffer final : public FramebufferBase
{
public:
    HYP_API VulkanFramebuffer(
        Vec2u extent,
        RenderPassStage stage = RenderPassStage::SHADER,
        uint32 num_multiview_layers = 0);

    HYP_API virtual ~VulkanFramebuffer() override;

    HYP_FORCE_INLINE const FixedArray<VkFramebuffer, max_frames_in_flight>& GetVulkanHandles() const
    {
        return m_handles;
    }

    HYP_FORCE_INLINE const VulkanRenderPassRef& GetRenderPass() const
    {
        return m_render_pass;
    }

    HYP_API virtual AttachmentRef AddAttachment(const AttachmentRef& attachment) override;
    HYP_API virtual AttachmentRef AddAttachment(uint32 binding, const ImageRef& image, LoadOperation load_op, StoreOperation store_op) override;

    HYP_API virtual AttachmentRef AddAttachment(
        uint32 binding,
        InternalFormat format,
        ImageType type,
        LoadOperation load_op,
        StoreOperation store_op) override;

    HYP_API virtual bool RemoveAttachment(uint32 binding) override;

    HYP_API virtual AttachmentBase* GetAttachment(uint32 binding) const override;

    HYP_FORCE_INLINE const VulkanAttachmentMap& GetAttachmentMap() const
    {
        return m_attachment_map;
    }

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_API virtual RendererResult Resize(Vec2u new_size) override;

    HYP_API virtual void BeginCapture(CommandBufferBase* command_buffer, uint32 frame_index) override;
    HYP_API virtual void EndCapture(CommandBufferBase* command_buffer, uint32 frame_index) override;

private:
    FixedArray<VkFramebuffer, max_frames_in_flight> m_handles;
    VulkanRenderPassRef m_render_pass;
    VulkanAttachmentMap m_attachment_map;
};

} // namespace renderer
} // namespace hyperion

#endif // RENDERER_FBO_HPP
