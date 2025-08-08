/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderAttachment.hpp>
#include <rendering/RenderObject.hpp>

#include <core/math/MathUtil.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanAttachment final : public AttachmentBase
{
public:
    VulkanAttachment(
        const VulkanGpuImageRef& image,
        const VulkanFramebufferWeakRef& framebuffer,
        RenderPassStage stage,
        LoadOperation loadOperation = LoadOperation::CLEAR,
        StoreOperation storeOperation = StoreOperation::STORE,
        BlendFunction blendFunction = BlendFunction::None());
    virtual ~VulkanAttachment() override;

    VkAttachmentReference GetVulkanHandle() const;
    VkAttachmentDescription GetVulkanAttachmentDescription() const;

    HYP_FORCE_INLINE RenderPassStage GetRenderPassStage() const
    {
        return m_stage;
    }

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;
    virtual RendererResult Destroy() override;

private:
    RenderPassStage m_stage;
};

} // namespace hyperion
