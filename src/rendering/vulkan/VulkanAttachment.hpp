/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderAttachment.hpp>
#include <rendering/RenderObject.hpp>

#include <core/math/MathUtil.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class VulkanAttachment final : public AttachmentBase
{
    HYP_OBJECT_BODY(VulkanAttachment);

public:
    VulkanAttachment(
        const VulkanGpuImageRef& image,
        const VulkanFramebufferWeakRef& framebuffer,
        RenderTargetType renderTargetType,
        LoadOperation loadOperation = LoadOperation::CLEAR,
        StoreOperation storeOperation = StoreOperation::STORE,
        BlendFunction blendFunction = BlendFunction::None());
    virtual ~VulkanAttachment() override;

    HYP_FORCE_INLINE const VkAttachmentReference& GetVulkanHandle() const
    {
        return m_vkAttachmentReference;
    }

    HYP_FORCE_INLINE const VkAttachmentDescription& GetVulkanAttachmentDescription() const
    {
        return m_vkAttachmentDescription;
    }

    HYP_FORCE_INLINE RenderTargetType GetRenderTargetType() const
    {
        return m_renderTargetType;
    }

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;

private:
    RenderTargetType m_renderTargetType;

    VkAttachmentReference m_vkAttachmentReference;
    VkAttachmentDescription m_vkAttachmentDescription;
};

} // namespace hyperion
