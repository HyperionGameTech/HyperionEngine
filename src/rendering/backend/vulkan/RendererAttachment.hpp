/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_ATTACHMENT_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_ATTACHMENT_HPP

#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <core/math/MathUtil.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class VulkanAttachment final : public AttachmentBase
{
public:
    HYP_API VulkanAttachment(
        const VulkanImageRef& image,
        const VulkanFramebufferWeakRef& framebuffer,
        RenderPassStage stage,
        LoadOperation load_operation = LoadOperation::CLEAR,
        StoreOperation store_operation = StoreOperation::STORE,
        BlendFunction blend_function = BlendFunction::None());
    HYP_API virtual ~VulkanAttachment() override;

    HYP_API VkAttachmentReference GetVulkanHandle() const;
    HYP_API VkAttachmentDescription GetVulkanAttachmentDescription() const;

    HYP_FORCE_INLINE RenderPassStage GetRenderPassStage() const
    {
        return m_stage;
    }

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

private:
    RenderPassStage m_stage;
};

} // namespace renderer
} // namespace hyperion

#endif
