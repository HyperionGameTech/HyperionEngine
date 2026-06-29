/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/GraphicsPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/Vulkan/VulkanPipeline.hpp>
#include <Rendering/Vulkan/VulkanGpuBuffer.hpp>
#include <Rendering/Vulkan/VulkanDescriptorSet.hpp>
#include <Rendering/Vulkan/VulkanCommandBuffer.hpp>
#include <Rendering/Vulkan/VulkanStructs.hpp>
#include <Rendering/Vulkan/VulkanShaderInstance.hpp>

#include <Rendering/RenderPipeline.hpp>
#include <Rendering/Device.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/Shared.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Math/Vector2.hpp>

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

#include <Vulkan/vulkan.h>

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
    VulkanGraphicsPipeline();
    explicit VulkanGraphicsPipeline(const VulkanShaderInstanceRef& shader);
    ~VulkanGraphicsPipeline();

    bool IsCreated() const override
    {
        return VulkanPipelineBase::IsCreated();
    }

    void Bind(VulkanCommandBuffer* cmd) override;
    void Bind(VulkanCommandBuffer* cmd, Vec2i viewportOffset, Vec2u viewportExtent) override;

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override;
#endif

    void UpdateDynamicStates(VulkanCommandBuffer* cmd) override
    {
        UpdateDynamicStates(cmd, /* onlyChanged */ true);
    }

    void UpdateDynamicStates(VulkanCommandBuffer* cmd, bool onlyChanged);

    static bool CanDynamicallySetDepthState();

private:
    RendererResult Rebuild() override;

    void BuildVertexAttributes(
        Array<VkVertexInputAttributeDescription, VulkanTempAllocator>& outVkVertexAttributes,
        Array<VkVertexInputBindingDescription, VulkanTempAllocator>& outVkVertexBindingDescriptions);

    void UpdateViewport(VulkanCommandBuffer* commandBuffer, const Viewport& viewport);

    Viewport m_viewport;
    VulkanRenderPass* m_renderPass;

    Array<VkDynamicState, VulkanAllocator> m_dynamicStates;
};

} // namespace Hyperion
