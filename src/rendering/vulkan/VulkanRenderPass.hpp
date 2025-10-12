/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once
#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanGpuImageView.hpp>
#include <rendering/vulkan/VulkanSampler.hpp>
#include <rendering/vulkan/VulkanAttachment.hpp>

#include <rendering/RenderObject.hpp>

#include <core/math/Vector4.hpp>
#include <core/containers/Array.hpp>

#include <core/Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

enum RenderPassMode
{
    RENDER_PASS_INLINE = 0,
    RENDER_PASS_SECONDARY_COMMAND_BUFFER = 1
};

HYP_CLASS(NoScriptBindings)
class VulkanRenderPass final : public HypObjectBase
{
    HYP_OBJECT_BODY(VulkanRenderPass);

public:
    VulkanRenderPass(RenderPassStage stage, RenderPassMode mode);
    VulkanRenderPass(RenderPassStage stage, RenderPassMode mode, uint32 numMultiviewLayers);
    virtual ~VulkanRenderPass() override;

    HYP_FORCE_INLINE VkRenderPass GetVulkanHandle() const
    {
        return m_handle;
    }

    RenderPassStage GetStage() const
    {
        return m_stage;
    }

    bool IsMultiview() const
    {
        return m_numMultiviewLayers > 1;
    }

    uint32 NumMultiviewLayers() const
    {
        return m_numMultiviewLayers;
    }

    void AddAttachment(VulkanAttachmentRef attachment);
    bool RemoveAttachment(const VulkanAttachment* attachment);

    const Array<VulkanAttachmentRef>& GetAttachments() const
    {
        return m_renderPassAttachments;
    }

    RendererResult Create();

    void Begin(VulkanCommandBuffer* cmd, VulkanFramebuffer* framebuffer);
    void End(VulkanCommandBuffer* cmd);

private:
    void CreateDependencies();

    void AddDependency(const VkSubpassDependency& dependency)
    {
        m_dependencies.PushBack(dependency);
    }

    RenderPassStage m_stage;
    RenderPassMode m_mode;
    uint32 m_numMultiviewLayers;

    Array<VulkanAttachmentRef> m_renderPassAttachments;

    Array<VkSubpassDependency> m_dependencies;
    Array<VkClearValue> m_vkClearValues;

    VkRenderPass m_handle;
};

using VulkanRenderPassRef = Handle<VulkanRenderPass>;
using VulkanRenderPassWeakRef = WeakHandle<VulkanRenderPass>;

} // namespace hyperion
