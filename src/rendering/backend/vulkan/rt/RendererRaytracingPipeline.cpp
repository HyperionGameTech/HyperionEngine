/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/rt/RendererRaytracingPipeline.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/RendererShader.hpp>
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <core/debug/Debug.hpp>
#include <core/math/MathUtil.hpp>
#include <core/math/Transform.hpp>

#include <Engine.hpp>

#include <cstring>

namespace hyperion {

extern IRenderingAPI* g_rendering_api;

namespace renderer {

static inline VulkanRenderingAPI* GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI*>(g_rendering_api);
}

static constexpr VkShaderStageFlags push_constant_stage_flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    | VK_SHADER_STAGE_ANY_HIT_BIT_KHR
    | VK_SHADER_STAGE_MISS_BIT_KHR
    | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

VulkanRaytracingPipeline::VulkanRaytracingPipeline()
    : VulkanPipelineBase(),
      RaytracingPipelineBase()
{
}

VulkanRaytracingPipeline::VulkanRaytracingPipeline(const VulkanShaderRef& shader, const VulkanDescriptorTableRef& descriptor_table)
    : VulkanPipelineBase(),
      RaytracingPipelineBase(shader, descriptor_table)
{
}

VulkanRaytracingPipeline::~VulkanRaytracingPipeline() = default;

RendererResult VulkanRaytracingPipeline::Create()
{
    if (!GetRenderingAPI()->GetDevice()->GetFeatures().IsRaytracingSupported())
    {
        return HYP_MAKE_ERROR(RendererError, "Raytracing is not supported on this device");
    }

    AssertThrow(m_shader != nullptr);

    RendererResult result;

    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    const uint32 max_set_layouts = GetRenderingAPI()->GetDevice()->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;

    Array<VkDescriptorSetLayout> used_layouts = GetPipelineVulkanDescriptorSetLayouts(*this);

    if (used_layouts.Size() > max_set_layouts)
    {
        DebugLog(
            LogType::Debug,
            "Device max bound descriptor sets exceeded (%llu > %u)\n",
            used_layouts.Size(),
            max_set_layouts);

        return HYP_MAKE_ERROR(RendererError, "Device max bound descriptor sets exceeded");
    }

    layout_info.setLayoutCount = uint32(used_layouts.Size());
    layout_info.pSetLayouts = used_layouts.Data();

    /* Push constants */
    const VkPushConstantRange push_constant_ranges[] = {
        { .stageFlags = push_constant_stage_flags,
            .offset = 0,
            .size = uint32(GetRenderingAPI()->GetDevice()->GetFeatures().PaddedSize<PushConstantData>()) }
    };

    layout_info.pushConstantRangeCount = ArraySize(push_constant_ranges);
    layout_info.pPushConstantRanges = push_constant_ranges;

    HYPERION_VK_PASS_ERRORS(
        vkCreatePipelineLayout(GetRenderingAPI()->GetDevice()->GetDevice(), &layout_info, VK_NULL_HANDLE, &m_layout),
        result);

    if (!result)
    {
        HYPERION_IGNORE_ERRORS(Destroy());

        return result;
    }

    VkRayTracingPipelineCreateInfoKHR pipeline_info { VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };

    const Array<VkPipelineShaderStageCreateInfo>& stages = static_cast<VulkanShader*>(m_shader.Get())->GetVulkanShaderStages();
    const Array<VulkanShaderGroup>& shader_groups = static_cast<VulkanShader*>(m_shader.Get())->GetShaderGroups();

    Array<VkRayTracingShaderGroupCreateInfoKHR> shader_group_create_infos;
    shader_group_create_infos.Resize(shader_groups.Size());

    for (SizeType i = 0; i < shader_groups.Size(); i++)
    {
        shader_group_create_infos[i] = shader_groups[i].raytracing_group_create_info;
    }

    pipeline_info.stageCount = uint32(stages.Size());
    pipeline_info.pStages = stages.Data();
    pipeline_info.groupCount = uint32(shader_group_create_infos.Size());
    pipeline_info.pGroups = shader_group_create_infos.Data();
    pipeline_info.layout = m_layout;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    HYPERION_VK_PASS_ERRORS(
        GetRenderingAPI()->GetDevice()->GetFeatures().dyn_functions.vkCreateRayTracingPipelinesKHR(
            GetRenderingAPI()->GetDevice()->GetDevice(),
            VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            VK_NULL_HANDLE,
            &m_handle),
        result);

    if (!result)
    {
        HYPERION_IGNORE_ERRORS(Destroy());

        return result;
    }

    HYPERION_PASS_ERRORS(CreateShaderBindingTables(static_cast<VulkanShader*>(m_shader.Get())), result);

    if (!result)
    {
        HYPERION_IGNORE_ERRORS(Destroy());

        return result;
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanRaytracingPipeline::Destroy()
{
    SafeRelease(std::move(m_shader));
    SafeRelease(std::move(m_descriptor_table));

    RendererResult result;

    for (auto& it : m_shader_binding_table_buffers)
    {
        HYPERION_PASS_ERRORS(it.second.buffer->Destroy(), result);
    }

    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(GetRenderingAPI()->GetDevice()->GetDevice(), m_handle, VK_NULL_HANDLE);
        m_handle = VK_NULL_HANDLE;
    }

    if (m_layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(GetRenderingAPI()->GetDevice()->GetDevice(), m_layout, VK_NULL_HANDLE);
        m_layout = VK_NULL_HANDLE;
    }

    return result;
}

void VulkanRaytracingPipeline::Bind(CommandBufferBase* command_buffer)
{
    vkCmdBindPipeline(
        static_cast<VulkanCommandBuffer*>(command_buffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        m_handle);

    if (m_push_constants)
    {
        vkCmdPushConstants(
            static_cast<VulkanCommandBuffer*>(command_buffer)->GetVulkanHandle(),
            m_layout,
            push_constant_stage_flags,
            0,
            m_push_constants.Size(),
            m_push_constants.Data());
    }
}

void VulkanRaytracingPipeline::TraceRays(CommandBufferBase* command_buffer, const Vec3u& extent) const
{
    GetRenderingAPI()->GetDevice()->GetFeatures().dyn_functions.vkCmdTraceRaysKHR(
        static_cast<VulkanCommandBuffer*>(command_buffer)->GetVulkanHandle(),
        &m_shader_binding_table_entries.ray_gen,
        &m_shader_binding_table_entries.ray_miss,
        &m_shader_binding_table_entries.closest_hit,
        &m_shader_binding_table_entries.callable,
        extent.x, extent.y, extent.z);
}

RendererResult VulkanRaytracingPipeline::CreateShaderBindingTables(VulkanShader* shader)
{
    const Array<VulkanShaderGroup>& shader_groups = shader->GetShaderGroups();

    const auto& features = GetRenderingAPI()->GetDevice()->GetFeatures();
    const auto& properties = features.GetRaytracingPipelineProperties();

    const uint32 handle_size = properties.shaderGroupHandleSize;
    const uint32 handle_size_aligned = GetRenderingAPI()->GetDevice()->GetFeatures().PaddedSize(handle_size, properties.shaderGroupHandleAlignment);
    const uint32 table_size = uint32(shader_groups.Size()) * handle_size_aligned;

    ByteBuffer shader_handle_storage(table_size);

    HYPERION_VK_CHECK(features.dyn_functions.vkGetRayTracingShaderGroupHandlesKHR(
        GetRenderingAPI()->GetDevice()->GetDevice(),
        m_handle,
        0,
        uint32(shader_groups.Size()),
        uint32(shader_handle_storage.Size()),
        shader_handle_storage.Data()));

    RendererResult result;

    uint32 offset = 0;

    ShaderBindingTableMap buffers;

    for (const auto& group : shader_groups)
    {
        const auto& create_info = group.raytracing_group_create_info;

        ShaderBindingTableEntry entry;

#define SHADER_PRESENT_IN_GROUP(type) (create_info.type != VK_SHADER_UNUSED_KHR ? 1 : 0)

        const uint32 shader_count = SHADER_PRESENT_IN_GROUP(generalShader)
            + SHADER_PRESENT_IN_GROUP(closestHitShader)
            + SHADER_PRESENT_IN_GROUP(anyHitShader)
            + SHADER_PRESENT_IN_GROUP(intersectionShader);

#undef SHADER_PRESENT_IN_GROUP

        AssertThrow(shader_count != 0);

        HYPERION_PASS_ERRORS(
            CreateShaderBindingTableEntry(
                shader_count,
                entry),
            result);

        if (result)
        {
            entry.buffer->Copy(
                handle_size,
                &shader_handle_storage[offset]);

            offset += handle_size;
        }
        else
        {
            for (auto& it : buffers)
            {
                HYPERION_IGNORE_ERRORS(it.second.buffer->Destroy());
            }

            return result;
        }

        buffers[group.type] = std::move(entry);
    }

    m_shader_binding_table_buffers = std::move(buffers);

#define GET_STRIDED_DEVICE_ADDRESS_REGION(type, out)                                       \
    do                                                                                     \
    {                                                                                      \
        auto it = m_shader_binding_table_buffers.find(type);                               \
        if (it != m_shader_binding_table_buffers.end())                                    \
        {                                                                                  \
            m_shader_binding_table_entries.out = it->second.strided_device_address_region; \
        }                                                                                  \
    }                                                                                      \
    while (0)

    GET_STRIDED_DEVICE_ADDRESS_REGION(ShaderModuleType::RAY_GEN, ray_gen);
    GET_STRIDED_DEVICE_ADDRESS_REGION(ShaderModuleType::RAY_MISS, ray_miss);
    GET_STRIDED_DEVICE_ADDRESS_REGION(ShaderModuleType::RAY_CLOSEST_HIT, closest_hit);

#undef GET_STRIDED_DEVICE_ADDRESS_REGION

    return result;
}

RendererResult VulkanRaytracingPipeline::CreateShaderBindingTableEntry(
    uint32 num_shaders,
    ShaderBindingTableEntry& out)
{
    const auto& properties = GetRenderingAPI()->GetDevice()->GetFeatures().GetRaytracingPipelineProperties();

    AssertThrow(properties.shaderGroupHandleSize != 0);

    if (num_shaders == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Creating shader binding table entry with zero shader count");
    }

    RendererResult result;

    out.buffer = MakeRenderObject<VulkanGPUBuffer>(GPUBufferType::SHADER_BINDING_TABLE, properties.shaderGroupHandleSize * num_shaders);

    HYPERION_PASS_ERRORS(out.buffer->Create(), result);

    if (result)
    {
        /* Get strided device address region */
        const uint32 handle_size = GetRenderingAPI()->GetDevice()->GetFeatures().PaddedSize(properties.shaderGroupHandleSize, properties.shaderGroupHandleAlignment);

        out.strided_device_address_region = VkStridedDeviceAddressRegionKHR {
            .deviceAddress = out.buffer->GetBufferDeviceAddress(),
            .stride = handle_size,
            .size = num_shaders * handle_size
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

} // namespace renderer
} // namespace hyperion
