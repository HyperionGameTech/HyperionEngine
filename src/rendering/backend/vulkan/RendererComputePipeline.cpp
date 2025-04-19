/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererComputePipeline.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/RendererShader.hpp>
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <core/debug/Debug.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Transform.hpp>

#include <cstring>

#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion {

extern IRenderingAPI *g_rendering_api;

namespace renderer {

static inline VulkanRenderingAPI *GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI *>(g_rendering_api);
}

VulkanComputePipeline::VulkanComputePipeline()
    : VulkanPipelineBase(),
      ComputePipelineBase()
{
}

VulkanComputePipeline::VulkanComputePipeline(const VulkanShaderRef &shader, const VulkanDescriptorTableRef &descriptor_table)
    : VulkanPipelineBase(),
      ComputePipelineBase(shader, descriptor_table)
{
}

VulkanComputePipeline::~VulkanComputePipeline()
{
}

void VulkanComputePipeline::Bind(CommandBufferBase *command_buffer)
{
    AssertThrow(m_handle != VK_NULL_HANDLE);

    vkCmdBindPipeline(
        static_cast<VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_handle
    );

    if (m_push_constants) {
        vkCmdPushConstants(
            static_cast<VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
            m_layout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            m_push_constants.Size(),
            m_push_constants.Data()
        );
    }
}

void VulkanComputePipeline::Dispatch(
    CommandBufferBase *command_buffer,
    const Vec3u &group_size
) const
{
    vkCmdDispatch(
        static_cast<VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
        group_size.x,
        group_size.y,
        group_size.z
    );
}

void VulkanComputePipeline::DispatchIndirect(
    CommandBufferBase *command_buffer,
    const GPUBufferRef &indirect_buffer,
    SizeType offset
) const
{
    vkCmdDispatchIndirect(
        static_cast<VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
        static_cast<VulkanGPUBuffer *>(indirect_buffer.Get())->GetVulkanHandle(),
        offset
    );
}

RendererResult VulkanComputePipeline::Create()
{
    /* Push constants */
    const VkPushConstantRange push_constant_ranges[] = {
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset     = 0,
            .size       = uint32(GetRenderingAPI()->GetDevice()->GetFeatures().PaddedSize<PushConstantData>())
        }
    };
    
    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    const Array<VkDescriptorSetLayout> used_layouts = GetPipelineVulkanDescriptorSetLayouts(*this);
    const uint32 max_set_layouts = GetRenderingAPI()->GetDevice()->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;

#if 0
    HYP_LOG(RenderingBackend, Debug, "Using {} descriptor set layouts in pipeline", used_layouts.Size());

    for (const DescriptorSetRef<Platform::VULKAN> &descriptor_set : m_descriptor_table->GetSets()[0]) {
        HYP_LOG(RenderingBackend, Debug, "\tDescriptor set layout: {} ({})",
            descriptor_set->GetLayout().GetName(), descriptor_set->GetLayout().GetDeclaration().set_index);

        for (const auto &it : descriptor_set->GetLayout().GetElements()) {
            HYP_LOG(RenderingBackend, Debug, "\t\tDescriptor: {}  binding: {}",
                it.first, it.second.binding);
        }
    }
#endif
    
    if (used_layouts.Size() > max_set_layouts) {
        return HYP_MAKE_ERROR(RendererError, "Device max bound descriptor sets exceeded");
    }

    layout_info.setLayoutCount = uint32(used_layouts.Size());
    layout_info.pSetLayouts = used_layouts.Data();
    layout_info.pushConstantRangeCount = uint32(std::size(push_constant_ranges));
    layout_info.pPushConstantRanges = push_constant_ranges;

    HYPERION_VK_CHECK_MSG(
        vkCreatePipelineLayout(GetRenderingAPI()->GetDevice()->GetDevice(), &layout_info, nullptr, &m_layout),
        "Failed to create compute pipeline layout"
    );

    VkComputePipelineCreateInfo pipeline_info { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };

    if (!m_shader) {
        return HYP_MAKE_ERROR(RendererError, "Compute shader not provided to pipeline");
    }

    const Array<VkPipelineShaderStageCreateInfo> &stages = static_cast<VulkanShader *>(m_shader.Get())->GetVulkanShaderStages();

    if (stages.Size() == 0) {
        return HYP_MAKE_ERROR(RendererError, "Compute pipelines must have at least one shader stage");
    }

    if (stages.Size() > 1) {
        return HYP_MAKE_ERROR(RendererError, "Compute pipelines must have only one shader stage");
    }

    pipeline_info.stage = stages.Front();
    pipeline_info.layout = m_layout;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    HYPERION_VK_CHECK_MSG(
        vkCreateComputePipelines(GetRenderingAPI()->GetDevice()->GetDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_handle),
        "Failed to create compute pipeline"
    );

    HYPERION_RETURN_OK;
}

RendererResult VulkanComputePipeline::Destroy()
{
    SafeRelease(std::move(m_shader));
    SafeRelease(std::move(m_descriptor_table));

    return VulkanPipelineBase::Destroy();
}

void VulkanComputePipeline::SetPushConstants(const void *data, SizeType size)
{
    VulkanPipelineBase::SetPushConstants(data, size);
}

} // namespace renderer
} // namespace hyperion
