/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/GraphicsPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/vulkan/VulkanPipeline.hpp>
#include <rendering/vulkan/VulkanGpuBuffer.hpp>
#include <rendering/vulkan/VulkanDescriptorSet.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>
#include <rendering/vulkan/VulkanShaderInstance.hpp>

#include <rendering/RenderPipeline.hpp>
#include <rendering/Device.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/Shared.hpp>

#include <core/containers/Array.hpp>

#include <core/math/Vector2.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

struct ShaderInputGroup;

class VulkanFramebuffer;
using VulkanFramebufferRef = Handle<VulkanFramebuffer>;
using VulkanFramebufferWeakRef = WeakHandle<VulkanFramebuffer>;

class VulkanRenderPass;
using VulkanRenderPassRef = Handle<VulkanRenderPass>;
using VulkanRenderPassWeakRef = WeakHandle<VulkanRenderPass>;

extern Pool* g_vulkanPool;

HYP_CLASS(NoScriptBindings)
class VulkanGraphicsPipeline final : public GraphicsPipelineBase, public VulkanPipelineBase
{
    HYP_OBJECT_BODY(VulkanGraphicsPipeline);

public:
    static Pool* GetAllocator() { return g_vulkanPool; }

    VulkanGraphicsPipeline();
    explicit VulkanGraphicsPipeline(const VulkanShaderRef& shader);
    ~VulkanGraphicsPipeline();

    bool IsCreated() const override
    {
        return VulkanPipelineBase::IsCreated();
    }

    void Bind(VulkanCommandBuffer* cmd) override;
    void Bind(VulkanCommandBuffer* cmd, Vec2i viewportOffset, Vec2u viewportExtent) override;

    void SetPushConstants(const void* data, SizeType size) override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    RendererResult Rebuild() override;

    void BuildVertexAttributes(
        const VertexAttributeSet& attributeSet,
        Array<VkVertexInputAttributeDescription>& outVkVertexAttributes,
        Array<VkVertexInputBindingDescription>& outVkVertexBindingDescriptions);

    void UpdateViewport(VulkanCommandBuffer* commandBuffer, const Viewport& viewport);

    Viewport m_viewport;
};

} // namespace Hyperion
