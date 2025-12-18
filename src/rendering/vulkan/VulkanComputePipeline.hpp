/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/ComputePipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/vulkan/VulkanPipeline.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class VulkanComputePipeline final : public ComputePipelineBase, public VulkanPipelineBase
{
    HYP_OBJECT_BODY(VulkanComputePipeline);

public:
    VulkanComputePipeline();
    VulkanComputePipeline(const VulkanShaderRef& shader, const VulkanDescriptorTableRef& descriptorTable);
    virtual ~VulkanComputePipeline() override;

    virtual bool IsCreated() const override
    {
        return VulkanPipelineBase::IsCreated();
    }

    virtual RendererResult Create() override;

    virtual void Bind(VulkanCommandBuffer* commandBuffer) override;

    virtual void Dispatch(VulkanCommandBuffer* commandBuffer, const Vec3u& groupSize) const override;
    virtual void DispatchIndirect(
        VulkanCommandBuffer* commandBuffer,
        const VulkanGpuBufferRef& indirectBuffer,
        SizeType offset = 0) const override;

    virtual void SetPushConstants(const void* data, SizeType size) override;

#ifdef HYP_DEBUG_MODE
    virtual void SetDebugName(Name name) override;
#endif
};

} // namespace hyperion
