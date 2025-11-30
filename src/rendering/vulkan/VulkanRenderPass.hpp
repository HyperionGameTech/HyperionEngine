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

constexpr ResourceState PreRenderResourceStates[2] = {
    // CLEAR=0, LOAD=1
    RS_UNDEFINED,    // CLEAR
    RS_RENDER_TARGET // LOAD
};

constexpr ResourceState PreRenderResourceStatesDepth[2] = {
    // CLEAR=0, LOAD=1
    RS_UNDEFINED,    // CLEAR
    RS_DEPTH_STENCIL // LOAD
};

constexpr ResourceState PostRenderResourceStates[RTT_MAX] = {
    RS_UNDEFINED,       // RTT_NONE
    RS_PRESENT,         // RTT_PRESENT
    RS_SHADER_RESOURCE, // RTT_SHADER_RESOURCE
    RS_RENDER_TARGET    // RTT_RENDER_TARGET
};

constexpr ResourceState PostRenderResourceStatesDepth[RTT_MAX] = {
    RS_UNDEFINED,       // RTT_NONE
    RS_DEPTH_STENCIL,   // RTT_PRESENT
    RS_SHADER_RESOURCE, // RTT_SHADER_RESOURCE
    RS_DEPTH_STENCIL    // RTT_RENDER_TARGET
};

enum RenderPassMode
{
    RENDER_PASS_INLINE = 0,
    RENDER_PASS_SECONDARY_COMMAND_BUFFER = 1
};

HYP_CLASS(NoScriptBindings)
class VulkanRenderPass final : public ObjectBase
{
    HYP_OBJECT_BODY(VulkanRenderPass);

public:
    VulkanRenderPass(RenderTargetType renderTargetType, RenderPassMode mode);
    VulkanRenderPass(RenderTargetType renderTargetType, RenderPassMode mode, uint32 numMultiviewLayers);
    virtual ~VulkanRenderPass() override;

    HYP_FORCE_INLINE VkRenderPass GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE RenderTargetType GetRenderTargetType() const
    {
        return m_renderTargetType;
    }

    HYP_FORCE_INLINE bool IsMultiview() const
    {
        return m_numMultiviewLayers > 1;
    }

    HYP_FORCE_INLINE uint32 NumMultiviewLayers() const
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

    RenderTargetType m_renderTargetType;
    RenderPassMode m_mode;
    uint32 m_numMultiviewLayers;

    Array<VulkanAttachmentRef> m_renderPassAttachments;

    Array<VkSubpassDependency, InlineAllocator<2>> m_dependencies;
    Array<VkClearValue, InlineAllocator<2>> m_vkClearValues;

    VkRenderPass m_handle;

    bool m_isRecording : 1;
};

using VulkanRenderPassRef = Handle<VulkanRenderPass>;
using VulkanRenderPassWeakRef = WeakHandle<VulkanRenderPass>;

} // namespace hyperion
