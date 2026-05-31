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

#include <Rendering/vulkan/VulkanPipeline.hpp>
#include <Rendering/vulkan/VulkanGpuBuffer.hpp>
#include <Rendering/vulkan/VulkanDescriptorSet.hpp>
#include <Rendering/vulkan/VulkanCommandBuffer.hpp>
#include <Rendering/vulkan/VulkanStructs.hpp>
#include <Rendering/vulkan/VulkanShaderInstance.hpp>

#include <Rendering/RenderPipeline.hpp>
#include <Rendering/Device.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/Shared.hpp>

#include <Core/containers/Array.hpp>

#include <Core/math/Vector2.hpp>

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

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
    VulkanGraphicsPipeline();
    explicit VulkanGraphicsPipeline(const VulkanShaderInstanceRef& shader);
    ~VulkanGraphicsPipeline();

    bool IsCreated() const override
    {
        return VulkanPipelineBase::IsCreated();
    }

    void Bind(VulkanCommandBuffer* cmd) override;
    void Bind(VulkanCommandBuffer* cmd, Vec2i viewportOffset, Vec2u viewportExtent) override;

#if HYP_DEBUG_MODE
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
        Array<VkVertexInputAttributeDescription>& outVkVertexAttributes,
        Array<VkVertexInputBindingDescription>& outVkVertexBindingDescriptions);

    void UpdateViewport(VulkanCommandBuffer* commandBuffer, const Viewport& viewport);

    Viewport m_viewport;
    VulkanRenderPass* m_renderPass;

    Array<VkDynamicState, VulkanAllocator> m_dynamicStates;
};

} // namespace Hyperion
