/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <system/Debug.hpp>
#include <math/MathUtil.hpp>
#include <math/Transform.hpp>

#include <cstring>

#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

ComputePipeline<Platform::VULKAN>::ComputePipeline()
    : Pipeline<Platform::VULKAN>()
{
}


ComputePipeline<Platform::VULKAN>::ComputePipeline(ShaderProgramRef<Platform::VULKAN> shader_program, DescriptorTableRef<Platform::VULKAN> descriptor_table)
    : Pipeline<Platform::VULKAN>(std::move(shader_program), std::move(descriptor_table))
{
}

ComputePipeline<Platform::VULKAN>::~ComputePipeline() = default;

void ComputePipeline<Platform::VULKAN>::Bind(CommandBuffer<Platform::VULKAN> *command_buffer) const
{
    AssertThrow(pipeline != nullptr);

    vkCmdBindPipeline(
        command_buffer->GetCommandBuffer(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline
    );

    vkCmdPushConstants(
        command_buffer->GetCommandBuffer(),
        layout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        static_cast<uint32>(sizeof(push_constants)),
        &push_constants
    );
}

void ComputePipeline<Platform::VULKAN>::SubmitPushConstants(CommandBuffer<Platform::VULKAN> *cmd) const
{
    vkCmdPushConstants(
        cmd->GetCommandBuffer(),
        layout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        static_cast<uint32>(sizeof(push_constants)),
        &push_constants
    );
}

void ComputePipeline<Platform::VULKAN>::Bind(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const PushConstantData &push_constant_data
)
{
    push_constants = push_constant_data;

    Bind(command_buffer);
}

void ComputePipeline<Platform::VULKAN>::Bind(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const void *push_constants_ptr,
    SizeType push_constants_size
)
{
    AssertThrow(push_constants_size <= sizeof(push_constants));

    Memory::MemCpy(&push_constants, push_constants_ptr, push_constants_size);

    if (push_constants_size < sizeof(push_constants)) {
        Memory::MemSet(&push_constants, 0, sizeof(push_constants) - push_constants_size);
    }

    Bind(command_buffer);
}

void ComputePipeline<Platform::VULKAN>::Dispatch(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    Extent3D group_size
) const
{
    vkCmdDispatch(
        command_buffer->GetCommandBuffer(),
        group_size.width,
        group_size.height,
        group_size.depth
    );
}

void ComputePipeline<Platform::VULKAN>::DispatchIndirect(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const IndirectBuffer<Platform::VULKAN> *indirect,
    SizeType offset
) const
{
    indirect->DispatchIndirect(command_buffer, offset);
}

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

    const Array<VkDescriptorSetLayout> used_layouts = GetDescriptorSetLayouts();
    const uint32 max_set_layouts = device->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;

    DebugLog(
        LogType::Debug,
        "Using %llu descriptor set layouts in pipeline\n",
        used_layouts.Size()
    );

    for (const DescriptorSet2Ref<Platform::VULKAN> &descriptor_set : m_descriptor_table->GetSets()[0]) {
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
        vkCreatePipelineLayout(device->GetDevice(), &layout_info, nullptr, &this->layout),
        "Failed to create compute pipeline layout"
    );

    VkComputePipelineCreateInfo pipeline_info { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };

    if (!m_shader_program) {
        return { Result::RENDERER_ERR, "Compute shader not provided to pipeline" };
    }

    const Array<VkPipelineShaderStageCreateInfo> &stages = m_shader_program->GetShaderStages();

    if (stages.Size() == 0) {
        return { Result::RENDERER_ERR, "Compute pipelines must have at least one shader stage" };
    }

    if (stages.Size() > 1) {
        return { Result::RENDERER_ERR, "Compute pipelines must have only one shader stage" };
    }

    pipeline_info.stage = stages.Front();
    pipeline_info.layout = layout;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    HYPERION_VK_CHECK_MSG(
        vkCreateComputePipelines(device->GetDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &this->pipeline),
        "Failed to create compute pipeline"
    );

    m_is_created = true;

    HYPERION_RETURN_OK;
}

Result ComputePipeline<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    DebugLog(LogType::Debug, "Destroying compute pipeline\n");

    m_is_created = false;
    
    SafeRelease(std::move(m_descriptor_table));

    vkDestroyPipeline(device->GetDevice(), this->pipeline, nullptr);
    this->pipeline = nullptr;

    vkDestroyPipelineLayout(device->GetDevice(), this->layout, nullptr);
    this->layout = nullptr;

    HYPERION_RETURN_OK;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
