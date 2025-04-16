/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_COMPUTE_PIPELINE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_COMPUTE_PIPELINE_HPP

#include <rendering/backend/RendererComputePipeline.hpp>

#include <rendering/backend/vulkan/RendererPipeline.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class VulkanComputePipeline final : public ComputePipelineBase, public VulkanPipelineBase
{
public:
    VulkanComputePipeline();
    VulkanComputePipeline(const VulkanShaderRef &shader, const VulkanDescriptorTableRef &descriptor_table);
    virtual ~VulkanComputePipeline() override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_API virtual void Bind(CommandBufferBase *command_buffer) override;

    HYP_API virtual void Dispatch(CommandBufferBase *command_buffer, const Vec3u &group_size) const override;
    HYP_API virtual void DispatchIndirect(
        CommandBufferBase *command_buffer,
        const GPUBufferRef &indirect_buffer,
        SizeType offset = 0
    ) const override;

    HYP_API virtual void SetPushConstants(const void *data, SizeType size) override;
};

} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_COMPUTE_PIPELINE_HPP
