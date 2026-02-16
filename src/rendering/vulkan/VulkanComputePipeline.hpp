/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/ComputePipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/vulkan/VulkanPipeline.hpp>

namespace Hyperion {

extern Pool* g_vulkanPool;

HYP_CLASS(NoScriptBindings)
class VulkanComputePipeline final : public ComputePipelineBase, public VulkanPipelineBase
{
    HYP_OBJECT_BODY(VulkanComputePipeline);

public:
    static Pool* GetAllocator() { return g_vulkanPool; }

    VulkanComputePipeline();
    explicit VulkanComputePipeline(const VulkanShaderInstanceRef& shader);
    ~VulkanComputePipeline() override;

    bool IsCreated() const override
    {
        return VulkanPipelineBase::IsCreated();
    }

    RendererResult Create() override;

    void Bind(VulkanCommandBuffer* commandBuffer) override;

    void Dispatch(VulkanCommandBuffer* commandBuffer, const Vec3u& groupSize) const override;
    void DispatchIndirect(
        VulkanCommandBuffer* commandBuffer,
        const VulkanGpuBufferRef& indirectBuffer,
        SizeType offset = 0) const override;

    void SetPushConstants(const void* data, SizeType size) override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif
};

} // namespace Hyperion
