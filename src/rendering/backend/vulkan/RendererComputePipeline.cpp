#include "RendererComputePipeline.hpp"
#include "RendererCommandBuffer.hpp"

#include <system/Debug.hpp>
#include <math/MathUtil.hpp>
#include <math/Transform.hpp>

#include <cstring>

#include "RendererFeatures.hpp"

namespace hyperion {
namespace renderer {
ComputePipeline::ComputePipeline()
    : Pipeline()
{
    static int x = 0;
    DebugLog(LogType::Debug, "Create Compute Pipeline [%d]\n", x++);
}

ComputePipeline::~ComputePipeline() = default;

void ComputePipeline::Bind(CommandBuffer *command_buffer) const
{
    vkCmdBindPipeline(
        command_buffer->GetCommandBuffer(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline
    );

    vkCmdPushConstants(
        command_buffer->GetCommandBuffer(),
        layout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(push_constants),
        &push_constants
    );
}

void ComputePipeline::Bind(CommandBuffer *command_buffer,
                           const PushConstantData &push_constant_data)
{
    push_constants = push_constant_data;

    Bind(command_buffer);
}

void ComputePipeline::Dispatch(CommandBuffer *command_buffer,
                               Extent3D group_size) const
{
    vkCmdDispatch(
        command_buffer->GetCommandBuffer(),
        group_size.width, group_size.height, group_size.depth
    );
}

void ComputePipeline::DispatchIndirect(CommandBuffer *command_buffer,
                                       const IndirectBuffer *indirect,
                                       size_t offset) const
{
    indirect->DispatchIndirect(command_buffer, offset);
}

Result ComputePipeline::Create(Device *device,
                               ShaderProgram *shader,
                               DescriptorPool *descriptor_pool)
{
    /* Push constants */
    const VkPushConstantRange push_constant_ranges[] = {
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = uint32_t(device->GetFeatures().PaddedSize<PushConstantData>())
        }
    };

    auto result = Result::OK;
   
    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

    auto used_layouts = GetDescriptorSetLayouts(device, descriptor_pool);
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

        return Result{Result::RENDERER_ERR, "Device max bound descriptor sets exceeded"};
    }

    layout_info.setLayoutCount = static_cast<uint32_t>(used_layouts.size());
    layout_info.pSetLayouts    = used_layouts.data();
    
    layout_info.pushConstantRangeCount = static_cast<uint32_t>(std::size(push_constant_ranges));
    layout_info.pPushConstantRanges    = push_constant_ranges;

    HYPERION_VK_CHECK_MSG(
        vkCreatePipelineLayout(device->GetDevice(), &layout_info, nullptr, &this->layout),
        "Failed to create compute pipeline layout"
    );

    VkComputePipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};

    const auto &stages = shader->GetShaderStages();
    AssertThrowMsg(stages.size() == 1, "Compute pipelines must have only one shader stage");

    pipeline_info.stage = stages.front();
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
