/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/Attachment.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/RenderObject.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

enum class VulkanRenderPassMode : uint8;

extern Pool* g_vulkanPool;

HYP_CLASS(NoScriptBindings)
class VulkanAttachment final : public AttachmentBase
{
    HYP_OBJECT_BODY(VulkanAttachment);

public:
    VulkanAttachment(
        const VulkanGpuImageRef& image,
        const VulkanFramebufferWeakRef& framebuffer,
        VulkanRenderPassMode renderPassMode,
        const AttachmentDesc& attachmentDesc);

    ~VulkanAttachment() override;

    HYP_FORCE_INLINE const VkAttachmentReference& GetVulkanHandle() const
    {
        return m_vkAttachmentReference;
    }

    HYP_FORCE_INLINE const VkAttachmentDescription& GetVulkanAttachmentDescription() const
    {
        return m_vkAttachmentDescription;
    }

    HYP_FORCE_INLINE VulkanRenderPassMode GetRenderPassMode() const
    {
        return m_renderPassMode;
    }

    bool IsCreated() const override;

    RendererResult Create() override;

private:
    VulkanRenderPassMode m_renderPassMode;

    VkAttachmentReference m_vkAttachmentReference;
    VkAttachmentDescription m_vkAttachmentDescription;
};

} // namespace Hyperion
