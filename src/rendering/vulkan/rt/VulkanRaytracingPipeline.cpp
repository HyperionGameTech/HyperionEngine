/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/vulkan/rt/VulkanRaytracingPipeline.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanShader.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

#include <core/debug/Debug.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Transform.hpp>

#include <engine/EngineDriver.hpp>

#include <cstring>

#include <VulkanRaytracingPipeline.generated.inl>

namespace hyperion {

extern VulkanRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return g_renderBackend;
}

template <>
Array<VkDescriptorSetLayout> GetVkDescriptorSetLayouts<VulkanRaytracingPipeline>(const VulkanRaytracingPipeline& pipeline)
{
    Array<VkDescriptorSetLayout> usedLayouts;

    VulkanShader* shader = pipeline.GetShader();
    AssertDebug(shader != nullptr && shader->GetCompiledShader() != nullptr);

    const DescriptorTableDeclaration* decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    Assert(decl != nullptr);

    for (const DescriptorSetDeclaration& setDecl : decl->elements)
    {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        Assert(GetRenderBackend()->GetOrCreateVkDescriptorSetLayout(DescriptorSetLayout(&setDecl), layout));

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

#pragma region VulkanRaytracingPipeline

VulkanRaytracingPipeline::VulkanRaytracingPipeline()
    : VulkanPipelineBase(),
      RaytracingPipelineBase()
{
}

VulkanRaytracingPipeline::VulkanRaytracingPipeline(const VulkanShaderRef& shader, const VulkanDescriptorTableRef& descriptorTable)
    : VulkanPipelineBase(),
      RaytracingPipelineBase(shader, descriptorTable)
{
}

VulkanRaytracingPipeline::~VulkanRaytracingPipeline()
{
    if (!IsCreated())
    {
        return;
    }

    SafeDelete(std::move(m_shader));
    SafeDelete(std::move(m_descriptorTable));

    m_shaderBindingTableBuffers.Clear();

    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, VK_NULL_HANDLE);
        m_handle = VK_NULL_HANDLE;
    }

    if (m_layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(GetRenderBackend()->GetDevice()->GetDevice(), m_layout, VK_NULL_HANDLE);
        m_layout = VK_NULL_HANDLE;
    }
}

RendererResult VulkanRaytracingPipeline::Create()
{
    if (!GetRenderBackend()->GetDevice()->GetFeatures().IsRaytracingSupported())
    {
        return HYP_MAKE_ERROR(RendererError, "Raytracing is not supported on this device");
    }

    HYP_GFX_ASSERT(m_shader != nullptr);

    RendererResult result;

    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layoutInfo { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    const uint32 maxSetLayouts = GetRenderBackend()->GetDevice()->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;

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
            .size = uint32(GetRenderBackend()->GetDevice()->GetFeatures().PaddedSize<PushConstantData>()) }
    };

    layoutInfo.pushConstantRangeCount = ArraySize(pushConstantRanges);
    layoutInfo.pPushConstantRanges = pushConstantRanges;

    VULKAN_CHECK(vkCreatePipelineLayout(GetRenderBackend()->GetDevice()->GetDevice(), &layoutInfo, VK_NULL_HANDLE, &m_layout));

    VkRayTracingPipelineCreateInfoKHR pipelineInfo { VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };

    const Array<VkPipelineShaderStageCreateInfo>& stages = m_shader->GetVulkanShaderStages();
    const Array<VulkanShaderGroup>& shaderGroups = m_shader->GetShaderGroups();

    Array<VkRayTracingShaderGroupCreateInfoKHR> shaderGroupCreateInfos;
    shaderGroupCreateInfos.Resize(shaderGroups.Size());

    for (SizeType i = 0; i < shaderGroups.Size(); i++)
    {
        shaderGroupCreateInfos[i] = shaderGroups[i].raytracingGroupCreateInfo;
    }

    pipelineInfo.stageCount = uint32(stages.Size());
    pipelineInfo.pStages = stages.Data();
    pipelineInfo.groupCount = uint32(shaderGroupCreateInfos.Size());
    pipelineInfo.pGroups = shaderGroupCreateInfos.Data();
    pipelineInfo.layout = m_layout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VULKAN_CHECK(g_vulkanDynamicFunctions->vkCreateRayTracingPipelinesKHR(
        GetRenderBackend()->GetDevice()->GetDevice(),
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

    HYPERION_PASS_ERRORS(CreateShaderBindingTables(m_shader), result);

    HYPERION_RETURN_OK;
}

void VulkanRaytracingPipeline::Bind(VulkanCommandBuffer* commandBuffer)
{
    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

    VULKAN_CAST(commandBuffer)->ResetBoundDescriptorSets();

    vkCmdBindPipeline(
        VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        m_handle);

    if (m_pushConstants)
    {
        vkCmdPushConstants(
            VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
            m_layout,
            PushConstantStageFlags,
            0,
            m_pushConstants.Size(),
            m_pushConstants.Data());
    }
}

void VulkanRaytracingPipeline::TraceRays(VulkanCommandBuffer* commandBuffer, const Vec3u& extent) const
{
    g_vulkanDynamicFunctions->vkCmdTraceRaysKHR(
        VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
        &m_shaderBindingTableEntries.rayGen,
        &m_shaderBindingTableEntries.rayMiss,
        &m_shaderBindingTableEntries.closestHit,
        &m_shaderBindingTableEntries.callable,
        extent.x, extent.y, extent.z);
}

RendererResult VulkanRaytracingPipeline::CreateShaderBindingTables(VulkanShader* shader)
{
    const Array<VulkanShaderGroup>& shaderGroups = shader->GetShaderGroups();

    const VulkanFeatures& features = GetRenderBackend()->GetDevice()->GetFeatures();
    const auto& properties = features.GetRaytracingPipelineProperties();

    const uint32 handleSize = properties.shaderGroupHandleSize;
    const uint32 handleSizeAligned = features.PaddedSize(handleSize, properties.shaderGroupHandleAlignment);
    const uint32 tableSize = uint32(shaderGroups.Size()) * handleSizeAligned;

    ByteBuffer shaderHandleStorage(tableSize);

    VULKAN_CHECK(g_vulkanDynamicFunctions->vkGetRayTracingShaderGroupHandlesKHR(
        GetRenderBackend()->GetDevice()->GetDevice(),
        m_handle,
        0,
        uint32(shaderGroups.Size()),
        uint32(shaderHandleStorage.Size()),
        shaderHandleStorage.Data()));

    RendererResult result;

    uint32 offset = 0;

    ShaderBindingTableMap buffers;

    for (const auto& group : shaderGroups)
    {
        const auto& createInfo = group.raytracingGroupCreateInfo;

        ShaderBindingTableEntry entry;

#define SHADER_PRESENT_IN_GROUP(type) (createInfo.type != VK_SHADER_UNUSED_KHR ? 1 : 0)

        const uint32 shaderCount = SHADER_PRESENT_IN_GROUP(generalShader)
            + SHADER_PRESENT_IN_GROUP(closestHitShader)
            + SHADER_PRESENT_IN_GROUP(anyHitShader)
            + SHADER_PRESENT_IN_GROUP(intersectionShader);

#undef SHADER_PRESENT_IN_GROUP

        HYP_GFX_ASSERT(shaderCount != 0);

        HYPERION_PASS_ERRORS(CreateShaderBindingTableEntry(shaderCount, entry), result);

        if (result)
        {
            entry.buffer->Copy(handleSize, &shaderHandleStorage[offset]);

            offset += handleSize;
        }
        else
        {
            for (auto& it : buffers)
            {
                it.second.buffer.Reset();
            }

            return result;
        }

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

    GET_STRIDED_DEVICE_ADDRESS_REGION(SMT_RAY_GEN, rayGen);
    GET_STRIDED_DEVICE_ADDRESS_REGION(SMT_RAY_MISS, rayMiss);
    GET_STRIDED_DEVICE_ADDRESS_REGION(SMT_RAY_CLOSEST_HIT, closestHit);

#undef GET_STRIDED_DEVICE_ADDRESS_REGION

    return result;
}

RendererResult VulkanRaytracingPipeline::CreateShaderBindingTableEntry(
    uint32 numShaders,
    ShaderBindingTableEntry& out)
{
    const auto& properties = GetRenderBackend()->GetDevice()->GetFeatures().GetRaytracingPipelineProperties();

    HYP_GFX_ASSERT(properties.shaderGroupHandleSize != 0);

    if (numShaders == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Creating shader binding table entry with zero shader count");
    }

    RendererResult result;

    out.buffer = CreateObject<VulkanGpuBuffer>(GpuBufferType::SHADER_BINDING_TABLE, properties.shaderGroupHandleSize * numShaders);
    out.buffer->SetDebugName(NAME("SBTBuffer"));

    HYPERION_PASS_ERRORS(out.buffer->Create(), result);

    if (result)
    {
        /* Get strided device address region */
        const uint32 handleSize = GetRenderBackend()->GetDevice()->GetFeatures().PaddedSize(properties.shaderGroupHandleSize, properties.shaderGroupHandleAlignment);

        out.stridedDeviceAddressRegion = VkStridedDeviceAddressRegionKHR {
            .deviceAddress = out.buffer->GetBufferDeviceAddress(),
            .stride = handleSize,
            .size = numShaders * handleSize
        };
    }
    else
    {
        out.buffer.Reset();
    }

    return result;
}

void VulkanRaytracingPipeline::SetPushConstants(const void* data, SizeType size)
{
    VulkanPipelineBase::SetPushConstants(data, size);
}

#pragma endregion VulkanRaytracingPipeline

} // namespace hyperion
