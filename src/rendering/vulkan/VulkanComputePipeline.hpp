/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderComputePipeline.hpp>

#include <rendering/vulkan/VulkanPipeline.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

class VulkanComputePipeline final : public ComputePipelineBase, public VulkanPipelineBase
{
public:
    VulkanComputePipeline();
    VulkanComputePipeline(const VulkanShaderRef& shader, const VulkanDescriptorTableRef& descriptorTable);
    virtual ~VulkanComputePipeline() override;
    
    virtual bool IsCreated() const override
    {
        return VulkanPipelineBase::IsCreated();
    }

    virtual RendererResult Create() override;
    virtual RendererResult Destroy() override;

    virtual void Bind(CommandBufferBase* commandBuffer) override;

    virtual void Dispatch(CommandBufferBase* commandBuffer, const Vec3u& groupSize) const override;
    virtual void DispatchIndirect(
        CommandBufferBase* commandBuffer,
        const GpuBufferRef& indirectBuffer,
        SizeType offset = 0) const override;

    virtual void SetPushConstants(const void* data, SizeType size) override;

#ifdef HYP_DEBUG_MODE
    virtual void SetDebugName(Name name) override;
#endif
};

} // namespace hyperion
