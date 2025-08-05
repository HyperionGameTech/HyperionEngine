/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <vulkan/vulkan.h>

#include <rendering/RenderGraphicsPipeline.hpp>

#include <rendering/vulkan/VulkanPipeline.hpp>
#include <rendering/vulkan/VulkanGpuBuffer.hpp>
#include <rendering/vulkan/VulkanDescriptorSet.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>

#include <rendering/RenderPipeline.hpp>
#include <rendering/RenderDevice.hpp>
#include <rendering/RenderShader.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/Shared.hpp>

#include <core/containers/Array.hpp>

#include <core/math/Vector2.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {

struct DescriptorTableDeclaration;

class VulkanFramebuffer;
using VulkanFramebufferRef = RenderObjectHandle_Strong<VulkanFramebuffer>;
using VulkanFramebufferWeakRef = RenderObjectHandle_Weak<VulkanFramebuffer>;

class VulkanRenderPass;
using VulkanRenderPassRef = RenderObjectHandle_Strong<VulkanRenderPass>;
using VulkanRenderPassWeakRef = RenderObjectHandle_Weak<VulkanRenderPass>;

class VulkanGraphicsPipeline final : public GraphicsPipelineBase, public VulkanPipelineBase
{
public:
    VulkanGraphicsPipeline();
    VulkanGraphicsPipeline(const VulkanShaderRef& shader, const VulkanDescriptorTableRef& descriptorTable);
    ~VulkanGraphicsPipeline();

    HYP_FORCE_INLINE const VulkanRenderPassRef& GetRenderPass() const
    {
        return m_renderPass;
    }

    void SetRenderPass(const VulkanRenderPassRef& renderPass);
    
    virtual bool IsCreated() const override
    {
        return VulkanPipelineBase::IsCreated();
    }

    virtual void Bind(CommandBufferBase* cmd) override;
    virtual void Bind(CommandBufferBase* cmd, Vec2i viewportOffset, Vec2u viewportExtent) override;

    virtual void SetPushConstants(const void* data, SizeType size) override;

    virtual RendererResult Destroy() override;

#ifdef HYP_DEBUG_MODE
    virtual void SetDebugName(Name name) override;
#endif

private:
    virtual RendererResult Rebuild() override;

    void BuildVertexAttributes(
        const VertexAttributeSet& attributeSet,
        Array<VkVertexInputAttributeDescription>& outVkVertexAttributes,
        Array<VkVertexInputBindingDescription>& outVkVertexBindingDescriptions);

    void UpdateViewport(VulkanCommandBuffer* commandBuffer, const Viewport& viewport);

    VulkanRenderPassRef m_renderPass;
    Viewport m_viewport;
};

} // namespace hyperion
