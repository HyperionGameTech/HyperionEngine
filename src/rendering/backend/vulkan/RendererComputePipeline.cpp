/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <core/system/Debug.hpp>
#include <math/MathUtil.hpp>
#include <math/Transform.hpp>

#include <cstring>

#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <>
ComputePipeline<Platform::VULKAN>::ComputePipeline()
    : Pipeline<Platform::VULKAN>()
{
}


template <>
ComputePipeline<Platform::VULKAN>::ComputePipeline(const ShaderRef<Platform::VULKAN> &shader, const DescriptorTableRef<Platform::VULKAN> &descriptor_table)
    : Pipeline<Platform::VULKAN>(shader, descriptor_table)
{
}

template <>
ComputePipeline<Platform::VULKAN>::~ComputePipeline()
{
}

template <>
void ComputePipeline<Platform::VULKAN>::Bind(CommandBuffer<Platform::VULKAN> *command_buffer) const
{
    AssertThrow(m_platform_impl.handle != VK_NULL_HANDLE);

    vkCmdBindPipeline(
        command_buffer->GetPlatformImpl().command_buffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_platform_impl.handle
    );

    if (m_push_constants) {
        vkCmdPushConstants(
            command_buffer->GetPlatformImpl().command_buffer,
            m_platform_impl.layout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            m_push_constants.Size(),
            m_push_constants.Data()
        );
    }
}

template <>
void ComputePipeline<Platform::VULKAN>::Dispatch(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    Extent3D group_size
) const
{
    vkCmdDispatch(
        command_buffer->GetPlatformImpl().command_buffer,
        group_size.width,
        group_size.height,
        group_size.depth
    );
}

template <>
void ComputePipeline<Platform::VULKAN>::DispatchIndirect(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const IndirectBuffer<Platform::VULKAN> *indirect,
    SizeType offset
) const
{
    vkCmdDispatchIndirect(
        command_buffer->GetPlatformImpl().command_buffer,
        indirect->GetPlatformImpl().handle,
        offset
    );
}

template <>
Result ComputePipeline<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    AssertThrowMsg(m_descriptor_table.IsValid(), "To use this function, you must use a DescriptorTable");

    /* Push constants */
    const VkPushConstantRange push_constant_ranges[] = {
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset     = 0,
            .size       = uint32(device->GetFeatures().PaddedSize<PushConstantData>())
        }
    };
    
    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    const Array<VkDescriptorSetLayout> used_layouts = m_platform_impl.GetDescriptorSetLayouts();
    const uint32 max_set_layouts = device->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;

    DebugLog(
        LogType::Debug,
        "Using %llu descriptor set layouts in pipeline\n",
        used_layouts.Size()
    );

    for (const DescriptorSetRef<Platform::VULKAN> &descriptor_set : m_descriptor_table->GetSets()[0]) {
        DebugLog(
            LogType::Debug,
            "\tDescriptor set layout: %s\n",
            descriptor_set->GetLayout().GetName().LookupString()
        );

        for (const auto &it : descriptor_set->GetLayout().GetElements()) {
            DebugLog(
                LogType::Debug,
                "\t\tDescriptor: %s  binding: %u\n",
                it.first.LookupString(),
                it.second.binding
            );
        }
    }
    
    if (used_layouts.Size() > max_set_layouts) {
        DebugLog(
            LogType::Debug,
            "Device max bound descriptor sets exceeded (%llu > %u)\n",
            used_layouts.Size(),
            max_set_layouts
        );

        return { Result::RENDERER_ERR, "Device max bound descriptor sets exceeded" };
    }

    layout_info.setLayoutCount = uint32(used_layouts.Size());
    layout_info.pSetLayouts = used_layouts.Data();
    layout_info.pushConstantRangeCount = uint32(std::size(push_constant_ranges));
    layout_info.pPushConstantRanges = push_constant_ranges;

    HYPERION_VK_CHECK_MSG(
        vkCreatePipelineLayout(device->GetDevice(), &layout_info, nullptr, &m_platform_impl.layout),
        "Failed to create compute pipeline layout"
    );

    VkComputePipelineCreateInfo pipeline_info { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };

    if (!m_shader) {
        return { Result::RENDERER_ERR, "Compute shader not provided to pipeline" };
    }

    const Array<VkPipelineShaderStageCreateInfo> &stages = m_shader->GetPlatformImpl().vk_shader_stages;

    if (stages.Size() == 0) {
        return { Result::RENDERER_ERR, "Compute pipelines must have at least one shader stage" };
    }

    if (stages.Size() > 1) {
        return { Result::RENDERER_ERR, "Compute pipelines must have only one shader stage" };
    }

    pipeline_info.stage = stages.Front();
    pipeline_info.layout = m_platform_impl.layout;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    HYPERION_VK_CHECK_MSG(
        vkCreateComputePipelines(device->GetDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_platform_impl.handle),
        "Failed to create compute pipeline"
    );

    HYPERION_RETURN_OK;
}

template <>
Result ComputePipeline<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    return Pipeline<Platform::VULKAN>::Destroy(device);
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
