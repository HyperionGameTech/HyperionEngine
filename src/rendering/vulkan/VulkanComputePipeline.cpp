/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanComputePipeline.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanShaderInstance.hpp>

#include <rendering/util/SafeDeleter.hpp>
#include <rendering/util/ShaderCompiler.hpp>

#include <core/debug/Debug.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Transform.hpp>

#include <cstring>

#include <VulkanComputePipeline.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface* g_renderInterface;

template <>
Array<VkDescriptorSetLayout, VulkanAllocator> GetVkDescriptorSetLayouts<VulkanComputePipeline>(const VulkanComputePipeline& pipeline)
{
    Array<VkDescriptorSetLayout, VulkanAllocator> usedLayouts;

    VulkanShaderInstance* shader = pipeline.GetShader();
    AssertDebug(shader != nullptr && shader->GetCompiledShader() != nullptr);

    const ShaderInputGroup* decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    Assert(decl != nullptr);

    for (const DescriptorSetDeclaration& setDecl : decl->elements)
    {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        Assert(g_renderInterface->GetOrCreateVkDescriptorSetLayout(DescriptorSetLayout(&setDecl), layout));

        Assert(layout != VK_NULL_HANDLE);

        usedLayouts.PushBack(layout);
    }

    return usedLayouts;
}

#pragma region VulkanComputePipeline

VulkanComputePipeline::VulkanComputePipeline()
    : VulkanPipelineBase(),
      ComputePipelineBase()
{
}

VulkanComputePipeline::VulkanComputePipeline(const VulkanShaderRef& shader)
    : VulkanPipelineBase(),
      ComputePipelineBase(shader)
{
}

VulkanComputePipeline::~VulkanComputePipeline()
{
    SafeDelete(std::move(m_shader));
}

void VulkanComputePipeline::Bind(VulkanCommandBuffer* commandBuffer)
{
    Assert(m_handle != VK_NULL_HANDLE);

    commandBuffer->ResetBoundDescriptorSets();

    vkCmdBindPipeline(
        commandBuffer->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_handle);

    if (m_pushConstants)
    {
        vkCmdPushConstants(
            commandBuffer->GetVulkanHandle(),
            m_layout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            m_pushConstants.Size(),
            m_pushConstants.Data());
    }
}

void VulkanComputePipeline::Dispatch(
    VulkanCommandBuffer* commandBuffer,
    const Vec3u& groupSize) const
{
    vkCmdDispatch(
        commandBuffer->GetVulkanHandle(),
        groupSize.x,
        groupSize.y,
        groupSize.z);
}

void VulkanComputePipeline::DispatchIndirect(
    VulkanCommandBuffer* commandBuffer,
    const VulkanGpuBufferRef& indirectBuffer,
    SizeType offset) const
{
    vkCmdDispatchIndirect(
        commandBuffer->GetVulkanHandle(),
        indirectBuffer->GetVulkanHandle(),
        offset);
}

RendererResult VulkanComputePipeline::Create()
{
    /* Push constants */
    const VkPushConstantRange pushConstantRanges[] = {
        { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = uint32(g_renderInterface->GetDevice()->GetFeatures().PaddedSize<PushConstantData>()) }
    };

    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layoutInfo { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    const Array<VkDescriptorSetLayout, VulkanAllocator> usedLayouts = GetVkDescriptorSetLayouts(*this);

    const uint32 maxSetLayouts = g_renderInterface->GetDevice()->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;

    if (usedLayouts.Size() > maxSetLayouts)
    {
        return HYP_MAKE_ERROR(RendererError, "Device max bound descriptor sets exceeded");
    }

    layoutInfo.setLayoutCount = uint32(usedLayouts.Size());
    layoutInfo.pSetLayouts = usedLayouts.Data();
    layoutInfo.pushConstantRangeCount = uint32(std::size(pushConstantRanges));
    layoutInfo.pPushConstantRanges = pushConstantRanges;

    VULKAN_CHECK_MSG(
        vkCreatePipelineLayout(g_renderInterface->GetDevice()->GetDevice(), &layoutInfo, nullptr, &m_layout),
        "Failed to create compute pipeline layout");

    VkComputePipelineCreateInfo pipelineInfo { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };

    if (!m_shader)
    {
        return HYP_MAKE_ERROR(RendererError, "Compute shader not provided to pipeline");
    }

    const Array<VkPipelineShaderStageCreateInfo, VulkanAllocator>& stages = m_shader->GetVulkanShaderStages();

    if (stages.Size() == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Compute pipelines must have at least one shader stage");
    }

    if (stages.Size() > 1)
    {
        return HYP_MAKE_ERROR(RendererError, "Compute pipelines must have only one shader stage");
    }

    pipelineInfo.stage = stages.Front();
    pipelineInfo.layout = m_layout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VULKAN_CHECK_MSG(
        vkCreateComputePipelines(g_renderInterface->GetDevice()->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_handle),
        "Failed to create compute pipeline");

#ifdef HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        VulkanPipelineBase::SetDebugName(debugName);
    }
#endif

    return {};
}

void VulkanComputePipeline::SetPushConstants(const void* data, SizeType size)
{
    VulkanPipelineBase::SetPushConstants(data, size);
}

#ifdef HYP_DEBUG_MODE

void VulkanComputePipeline::SetDebugName(Name name)
{
    ComputePipelineBase::SetDebugName(name);

    if (!IsCreated())
    {
        return;
    }

    VulkanPipelineBase::SetDebugName(name);
}

#endif

#pragma endregion VulkanComputePipeline

} // namespace Hyperion
