/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/RenderGraphicsPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

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

#include <core/HashCode.hpp>
#include <core/Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

struct DescriptorTableDeclaration;

class VulkanFramebuffer;
using VulkanFramebufferRef = Handle<VulkanFramebuffer>;
using VulkanFramebufferWeakRef = WeakHandle<VulkanFramebuffer>;

class VulkanRenderPass;
using VulkanRenderPassRef = Handle<VulkanRenderPass>;
using VulkanRenderPassWeakRef = WeakHandle<VulkanRenderPass>;

HYP_CLASS(NoScriptBindings)
class VulkanGraphicsPipeline final : public GraphicsPipelineBase, public VulkanPipelineBase
{
    HYP_OBJECT_BODY(VulkanGraphicsPipeline);

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

    virtual void Bind(VulkanCommandBuffer* cmd) override;
    virtual void Bind(VulkanCommandBuffer* cmd, Vec2i viewportOffset, Vec2u viewportExtent) override;

    virtual void SetPushConstants(const void* data, SizeType size) override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
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
