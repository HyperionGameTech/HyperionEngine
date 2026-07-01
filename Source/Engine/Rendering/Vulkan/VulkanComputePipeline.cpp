/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <Rendering/Vulkan/VulkanComputePipeline.hpp>
#include <Rendering/Vulkan/VulkanCommandBuffer.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#include <Rendering/Vulkan/VulkanFeatures.hpp>
#include <Rendering/Vulkan/VulkanDevice.hpp>
#include <Rendering/Vulkan/VulkanShaderInstance.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderCompiler.hpp>

#include <Rendering/Shader.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Transform.hpp>

#include <cstring>

#include <VulkanComputePipeline.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface RI;

template <>
Array<VkDescriptorSetLayout, VulkanAllocator> GetVkDescriptorSetLayouts<VulkanComputePipeline>(const VulkanComputePipeline& pipeline)
{
    Array<VkDescriptorSetLayout, VulkanAllocator> usedLayouts;

    VulkanShaderInstance* shaderInstance = pipeline.GetShader();
    AssertDebug(shaderInstance != nullptr && shaderInstance->GetShader() != nullptr);

    const ShaderInputGroup* decl = shaderInstance->GetShader()->GetDescriptorTableDeclaration();
    Assert(decl != nullptr);

    for (const ShaderInputSet& setDecl : decl->elements)
    {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        Assert(RI.GetOrCreateVkDescriptorSetLayout(DescriptorSetLayout(&setDecl), layout));

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

VulkanComputePipeline::VulkanComputePipeline(const VulkanShaderInstanceRef& shaderInstance)
    : VulkanPipelineBase(),
      ComputePipelineBase(shaderInstance)
{
}

VulkanComputePipeline::~VulkanComputePipeline()
{
    m_shaderInstance.Reset();
}

void VulkanComputePipeline::Bind(VulkanCommandBuffer* commandBuffer)
{
    Assert(m_handle != VK_NULL_HANDLE);

    commandBuffer->ResetBoundDescriptorSets();

    vkCmdBindPipeline(
        commandBuffer->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_handle);
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
    size_t offset) const
{
    vkCmdDispatchIndirect(
        commandBuffer->GetVulkanHandle(),
        indirectBuffer->GetVulkanHandle(),
        offset);
}

RendererResult VulkanComputePipeline::Create()
{

    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layoutInfo { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    const Array<VkDescriptorSetLayout, VulkanAllocator> usedLayouts = GetVkDescriptorSetLayouts(*this);

    const uint32 maxSetLayouts = RI.GetDevice()->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;

    if (usedLayouts.Size() > maxSetLayouts)
    {
        return HYP_MAKE_ERROR(RendererError, "Device max bound descriptor sets exceeded");
    }

    layoutInfo.setLayoutCount = uint32(usedLayouts.Size());
    layoutInfo.pSetLayouts = usedLayouts.Data();
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges = nullptr;

    VULKAN_CHECK_MSG(
        vkCreatePipelineLayout(RI.GetDevice()->GetDevice(), &layoutInfo, nullptr, &m_layout),
        "Failed to create compute pipeline layout");

    VkComputePipelineCreateInfo pipelineInfo { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };

    if (!m_shaderInstance)
    {
        return HYP_MAKE_ERROR(RendererError, "Compute shader not provided to pipeline");
    }

    const Array<VkPipelineShaderStageCreateInfo, VulkanAllocator>& stages = m_shaderInstance->GetVulkanShaderStages();

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
        vkCreateComputePipelines(RI.GetDevice()->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_handle),
        "Failed to create compute pipeline");

#ifdef HYP_RHI_DEBUG_NAMES
    if (Name debugName = GetDebugName())
    {
        SetDebugNameLayout(debugName);
        VulkanPipelineBase::SetDebugName(debugName);
    }
#endif

    return {};
}

#ifdef HYP_RHI_DEBUG_NAMES
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
