/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once
#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanGpuImageView.hpp>
#include <rendering/vulkan/VulkanSampler.hpp>
#include <rendering/vulkan/VulkanAttachment.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <core/math/Vector4.hpp>
#include <core/containers/Array.hpp>

#include <core/Types.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class VulkanRenderPass final : public ObjectBase
{
    HYP_OBJECT_BODY(VulkanRenderPass);

public:
    VulkanRenderPass(RenderTargetType renderTargetType, RenderPassMode mode);
    VulkanRenderPass(RenderTargetType renderTargetType, RenderPassMode mode, const RenderTargetDesc& renderTargetDesc);
    ~VulkanRenderPass() override;

    HYP_FORCE_INLINE VkRenderPass GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE RenderTargetType GetRenderTargetType() const
    {
        return m_renderTargetType;
    }

    HYP_FORCE_INLINE RenderTargetDesc& GetRenderTargetDesc()
    {
        return m_renderTargetDesc;
    }

    HYP_FORCE_INLINE const RenderTargetDesc& GetRenderTargetDesc() const
    {
        return m_renderTargetDesc;
    }

    HYP_FORCE_INLINE bool IsMultiview() const
    {
        return m_renderTargetDesc.numViews > 1;
    }

    HYP_FORCE_INLINE uint32 NumMultiviewLayers() const
    {
        return m_renderTargetDesc.numViews;
    }

    const Array<AttachmentDesc>& GetAttachmentDescs() const
    {
        return m_renderTargetDesc.attachments;
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

    RenderTargetType m_renderTargetType;
    RenderPassMode m_mode;
    
    RenderTargetDesc m_renderTargetDesc;

    Array<VkSubpassDependency, InlineAllocator<2>> m_dependencies;
    Array<VkClearValue, InlineAllocator<2>> m_vkClearValues;

    VkRenderPass m_handle;

    VulkanFramebuffer* m_recordingFramebuffer;
};

using VulkanRenderPassRef = Handle<VulkanRenderPass>;
using VulkanRenderPassWeakRef = WeakHandle<VulkanRenderPass>;

} // namespace Hyperion
