/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/ComputePipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12ComputePipeline final : public ComputePipelineBase
{
    HYP_OBJECT_BODY(DX12ComputePipeline);

public:
    DX12ComputePipeline();
    DX12ComputePipeline(const DX12ShaderRef& shader, const DX12DescriptorTableRef& descriptorTable);
    virtual ~DX12ComputePipeline() override;

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;

    virtual void Bind(CommandBuffer* commandBuffer) override;

    virtual void Dispatch(CommandBuffer* commandBuffer, const Vec3u& groupSize) const override;
    virtual void DispatchIndirect(
        CommandBuffer* commandBuffer,
        const DX12GpuBufferRef& indirectBuffer,
        SizeType offset = 0) const override;

    virtual void SetPushConstants(const void* data, SizeType size) override;

#ifdef HYP_DEBUG_MODE
    virtual void SetDebugName(Name name) override;
#endif
};

} // namespace Hyperion
