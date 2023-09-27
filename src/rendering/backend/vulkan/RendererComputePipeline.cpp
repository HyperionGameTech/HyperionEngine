#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <system/Debug.hpp>
#include <math/MathUtil.hpp>
#include <math/Transform.hpp>

#include <cstring>

#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion {
namespace renderer {

ComputePipeline::ComputePipeline()
    : Pipeline()
{
}

ComputePipeline::ComputePipeline(const Array<const DescriptorSet *> &used_descriptor_sets)
    : Pipeline(used_descriptor_sets)
{
}

ComputePipeline::~ComputePipeline() = default;

void ComputePipeline::Bind(CommandBuffer *command_buffer) const
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
        static_cast<UInt32>(sizeof(push_constants)),
        &push_constants
    );
}

void ComputePipeline::SubmitPushConstants(CommandBuffer *cmd) const
{
    vkCmdPushConstants(
        cmd->GetCommandBuffer(),
        layout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        static_cast<UInt32>(sizeof(push_constants)),
        &push_constants
    );
}

void ComputePipeline::Bind(
    CommandBuffer *command_buffer,
    const PushConstantData &push_constant_data
)
{
    push_constants = push_constant_data;

    Bind(command_buffer);
}

void ComputePipeline::Bind(
    CommandBuffer *command_buffer,
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

void ComputePipeline::Dispatch(
    CommandBuffer *command_buffer,
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

void ComputePipeline::DispatchIndirect(
    CommandBuffer *command_buffer,
    const IndirectBuffer *indirect,
    SizeType offset
) const
{
    indirect->DispatchIndirect(command_buffer, offset);
}

Result ComputePipeline::Create(
    Device *device,
    ShaderProgram *shader,
    DescriptorPool *descriptor_pool
)
{
    /* Push constants */
    const VkPushConstantRange push_constant_ranges[] = {
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = uint32_t(device->GetFeatures().PaddedSize<PushConstantData>())
        }
    };
    
    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    const auto used_layouts = GetDescriptorSetLayouts(device, descriptor_pool);
    const auto max_set_layouts = device->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;

    DebugLog(
        LogType::Debug,
        "Using %llu descriptor set layouts in pipeline\n",
        used_layouts.size()
    );
    
    if (used_layouts.size() > max_set_layouts) {
        DebugLog(
            LogType::Debug,
            "Device max bound descriptor sets exceeded (%llu > %u)\n",
            used_layouts.size(),
            max_set_layouts
        );

        return { Result::RENDERER_ERR, "Device max bound descriptor sets exceeded" };
    }

    layout_info.setLayoutCount = static_cast<UInt32>(used_layouts.size());
    layout_info.pSetLayouts = used_layouts.data();
    
    layout_info.pushConstantRangeCount = static_cast<UInt32>(std::size(push_constant_ranges));
    layout_info.pPushConstantRanges = push_constant_ranges;

    HYPERION_VK_CHECK_MSG(
        vkCreatePipelineLayout(device->GetDevice(), &layout_info, nullptr, &this->layout),
        "Failed to create compute pipeline layout"
    );

    VkComputePipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};

    if (shader == nullptr) {
        return { Result::RENDERER_ERR, "Compute shader not provided to pipeline" };
    }

    const auto &stages = shader->GetShaderStages();

    if (stages.Size() != 1) {
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

    HYPERION_RETURN_OK;
}

Result ComputePipeline::Destroy(Device *device)
{
    DebugLog(LogType::Debug, "Destroying compute pipeline\n");

    vkDestroyPipeline(device->GetDevice(), this->pipeline, nullptr);
    this->pipeline = nullptr;

    vkDestroyPipelineLayout(device->GetDevice(), this->layout, nullptr);
    this->layout = nullptr;

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion
