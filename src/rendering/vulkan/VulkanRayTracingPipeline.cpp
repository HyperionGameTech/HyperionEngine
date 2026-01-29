/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanRayTracingPipeline.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanShader.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>

#include <rendering/util/SafeDeleter.hpp>
#include <rendering/util/ShaderCompiler.hpp>

#include <core/debug/Debug.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Transform.hpp>

#include <engine/EngineDriver.hpp>

#include <VulkanRayTracingPipeline.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface* g_renderInterface;

template <>
Array<VkDescriptorSetLayout> GetVkDescriptorSetLayouts<VulkanRayTracingPipeline>(const VulkanRayTracingPipeline& pipeline)
{
    Array<VkDescriptorSetLayout> usedLayouts;

    VulkanShader* shader = pipeline.GetShader();
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

static constexpr VkShaderStageFlags PushConstantStageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    | VK_SHADER_STAGE_ANY_HIT_BIT_KHR
    | VK_SHADER_STAGE_MISS_BIT_KHR
    | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

#pragma region VulkanRayTracingPipeline

VulkanRayTracingPipeline::VulkanRayTracingPipeline()
    : VulkanPipelineBase(),
      RayTracingPipelineBase()
{
}

VulkanRayTracingPipeline::VulkanRayTracingPipeline(const VulkanShaderRef& shader)
    : VulkanPipelineBase(),
      RayTracingPipelineBase(shader)
{
}

VulkanRayTracingPipeline::~VulkanRayTracingPipeline()
{
    if (!IsCreated())
    {
        return;
    }

    SafeDelete(std::move(m_shader));

    m_shaderBindingTableBuffers.Clear();
}

RendererResult VulkanRayTracingPipeline::Create()
{
    if (!g_renderInterface->GetDevice()->GetFeatures().IsRayTracingSupported())
    {
        return HYP_MAKE_ERROR(RendererError, "RayTracing is not supported on this device");
    }

    Assert(m_shader != nullptr);

    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layoutInfo { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    const uint32 maxSetLayouts = g_renderInterface->GetDevice()->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;

    Array<VkDescriptorSetLayout> usedLayouts = GetVkDescriptorSetLayouts(*this);

    if (usedLayouts.Size() > maxSetLayouts)
    {
        return HYP_MAKE_ERROR(RendererError, "Device max bound descriptor sets exceeded");
    }

    layoutInfo.setLayoutCount = uint32(usedLayouts.Size());
    layoutInfo.pSetLayouts = usedLayouts.Data();

    /* Push constants */
    const VkPushConstantRange pushConstantRanges[] = {
        { .stageFlags = PushConstantStageFlags,
            .offset = 0,
            .size = uint32(g_renderInterface->GetDevice()->GetFeatures().PaddedSize<PushConstantData>()) }
    };

    layoutInfo.pushConstantRangeCount = ArraySize(pushConstantRanges);
    layoutInfo.pPushConstantRanges = pushConstantRanges;

    VULKAN_CHECK(vkCreatePipelineLayout(g_renderInterface->GetDevice()->GetDevice(), &layoutInfo, VK_NULL_HANDLE, &m_layout));

    VkRayTracingPipelineCreateInfoKHR pipelineInfo { VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };

    const Array<VkPipelineShaderStageCreateInfo>& stages = m_shader->GetVulkanShaderStages();
    const Array<VulkanShaderGroup>& shaderGroups = m_shader->GetShaderGroups();

    Array<VkRayTracingShaderGroupCreateInfoKHR> shaderGroupCreateInfos;
    shaderGroupCreateInfos.Resize(shaderGroups.Size());

    for (SizeType i = 0; i < shaderGroups.Size(); i++)
    {
        shaderGroupCreateInfos[i] = shaderGroups[i].rayTracingGroupCreateInfo;
    }

    pipelineInfo.stageCount = uint32(stages.Size());
    pipelineInfo.pStages = stages.Data();
    pipelineInfo.groupCount = uint32(shaderGroupCreateInfos.Size());
    pipelineInfo.pGroups = shaderGroupCreateInfos.Data();
    pipelineInfo.layout = m_layout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VULKAN_CHECK(g_vulkanDynamicFunctions->vkCreateRayTracingPipelinesKHR(
        g_renderInterface->GetDevice()->GetDevice(),
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        1,
        &pipelineInfo,
        VK_NULL_HANDLE,
        &m_handle));

#ifdef HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        SetDebugName(debugName);
    }
#endif

    CheckResultOrReturn(CreateShaderBindingTables(m_shader));

    return {};
}

void VulkanRayTracingPipeline::Bind(VulkanCommandBuffer* commandBuffer)
{
    Assert(m_handle != VK_NULL_HANDLE);

    commandBuffer->ResetBoundDescriptorSets();

    vkCmdBindPipeline(
        commandBuffer->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        m_handle);

    if (m_pushConstants)
    {
        vkCmdPushConstants(
            commandBuffer->GetVulkanHandle(),
            m_layout,
            PushConstantStageFlags,
            0,
            m_pushConstants.Size(),
            m_pushConstants.Data());
    }
}

void VulkanRayTracingPipeline::TraceRays(VulkanCommandBuffer* commandBuffer, const Vec3u& extent) const
{
    g_vulkanDynamicFunctions->vkCmdTraceRaysKHR(
        commandBuffer->GetVulkanHandle(),
        &m_shaderBindingTableEntries.rayGen,
        &m_shaderBindingTableEntries.rayMiss,
        &m_shaderBindingTableEntries.closestHit,
        &m_shaderBindingTableEntries.callable,
        extent.x, extent.y, extent.z);
}

RendererResult VulkanRayTracingPipeline::CreateShaderBindingTables(VulkanShader* shader)
{
    const Array<VulkanShaderGroup>& shaderGroups = shader->GetShaderGroups();

    const VulkanFeatures& features = g_renderInterface->GetDevice()->GetFeatures();
    const auto& properties = features.GetRayTracingPipelineProperties();

    const uint32 handleSize = properties.shaderGroupHandleSize;
    const uint32 handleSizeAligned = features.PaddedSize(handleSize, properties.shaderGroupHandleAlignment);
    const uint32 tableSize = uint32(shaderGroups.Size()) * handleSizeAligned;

    ByteBuffer shaderHandleStorage(tableSize);

    VULKAN_CHECK(g_vulkanDynamicFunctions->vkGetRayTracingShaderGroupHandlesKHR(
        g_renderInterface->GetDevice()->GetDevice(),
        m_handle,
        0,
        uint32(shaderGroups.Size()),
        uint32(shaderHandleStorage.Size()),
        shaderHandleStorage.Data()));

    uint32 offset = 0;

    ShaderBindingTableMap buffers;

    for (const auto& group : shaderGroups)
    {
        const auto& createInfo = group.rayTracingGroupCreateInfo;

        ShaderBindingTableEntry entry;

#define SHADER_PRESENT_IN_GROUP(type) (createInfo.type != VK_SHADER_UNUSED_KHR ? 1 : 0)

        const uint32 shaderCount = SHADER_PRESENT_IN_GROUP(generalShader)
            + SHADER_PRESENT_IN_GROUP(closestHitShader)
            + SHADER_PRESENT_IN_GROUP(anyHitShader)
            + SHADER_PRESENT_IN_GROUP(intersectionShader);

#undef SHADER_PRESENT_IN_GROUP

        Assert(shaderCount != 0);

        CheckResultOrReturn(CreateShaderBindingTableEntry(shaderCount, entry));

        entry.buffer->Copy(handleSize, &shaderHandleStorage[offset]);

        offset += handleSize;

        buffers[group.type] = std::move(entry);
    }

    m_shaderBindingTableBuffers = std::move(buffers);

#define GET_STRIDED_DEVICE_ADDRESS_REGION(type, out)                                 \
    do                                                                               \
    {                                                                                \
        auto it = m_shaderBindingTableBuffers.Find(type);                            \
        if (it != m_shaderBindingTableBuffers.End())                                 \
        {                                                                            \
            m_shaderBindingTableEntries.out = it->second.stridedDeviceAddressRegion; \
        }                                                                            \
    }                                                                                \
    while (0)

    GET_STRIDED_DEVICE_ADDRESS_REGION(ShaderModuleType::RayGen, rayGen);
    GET_STRIDED_DEVICE_ADDRESS_REGION(ShaderModuleType::Miss, rayMiss);
    GET_STRIDED_DEVICE_ADDRESS_REGION(ShaderModuleType::ClosestHit, closestHit);

#undef GET_STRIDED_DEVICE_ADDRESS_REGION

    return {};
}

RendererResult VulkanRayTracingPipeline::CreateShaderBindingTableEntry(
    uint32 numShaders,
    ShaderBindingTableEntry& out)
{
    const auto& properties = g_renderInterface->GetDevice()->GetFeatures().GetRayTracingPipelineProperties();

    Assert(properties.shaderGroupHandleSize != 0);

    if (numShaders == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Creating shader binding table entry with zero shader count");
    }

    out.buffer = MakeHandle<VulkanGpuBuffer>(GpuBufferType::SHADER_BINDING_TABLE, properties.shaderGroupHandleSize * numShaders);
    out.buffer->SetDebugName(NAME("SBTBuffer"));

    CheckResultOrReturn(out.buffer->Create());

    /* Get strided device address region */
    const uint32 handleSize = g_renderInterface->GetDevice()->GetFeatures().PaddedSize(properties.shaderGroupHandleSize, properties.shaderGroupHandleAlignment);

    out.stridedDeviceAddressRegion = VkStridedDeviceAddressRegionKHR {
        .deviceAddress = out.buffer->GetBufferDeviceAddress(),
        .stride = handleSize,
        .size = numShaders * handleSize
    };

    return {};
}

void VulkanRayTracingPipeline::SetPushConstants(const void* data, SizeType size)
{
    VulkanPipelineBase::SetPushConstants(data, size);
}

#pragma endregion VulkanRayTracingPipeline

} // namespace Hyperion
