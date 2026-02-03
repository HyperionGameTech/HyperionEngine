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

enum class VulkanRenderPassMode : uint8;

extern Pool* g_vulkanPool;

HYP_CLASS(NoScriptBindings)
class VulkanRenderPass final : public ObjectBase
{
    HYP_OBJECT_BODY(VulkanRenderPass);

public:
    static Pool* GetAllocator() { return g_vulkanPool; }

    VulkanRenderPass(
        const RenderTargetDesc& renderTargetDesc,
        VulkanRenderPassMode renderPassMode);
    ~VulkanRenderPass() override;

    HYP_FORCE_INLINE VkRenderPass GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VulkanRenderPassMode GetRenderPassMode() const
    {
        return m_renderPassMode;
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
        return m_renderTargetDesc.numLayers > 1;
    }

    HYP_FORCE_INLINE uint32 NumMultiviewLayers() const
    {
        return m_renderTargetDesc.numLayers;
    }

    Span<const AttachmentDesc> GetAttachmentDescs() const
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
    
    RenderTargetDesc m_renderTargetDesc;
    VulkanRenderPassMode m_renderPassMode;
    
    Array<VkSubpassDependency, VulkanAllocator> m_dependencies;
    Array<VkClearValue, VulkanAllocator> m_vkClearValues;

    VkRenderPass m_handle;

    VulkanFramebuffer* m_recordingFramebuffer;
};

using VulkanRenderPassRef = Handle<VulkanRenderPass>;
using VulkanRenderPassWeakRef = WeakHandle<VulkanRenderPass>;

} // namespace Hyperion
