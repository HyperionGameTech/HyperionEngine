/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12ComputePipeline.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>

#include <DX12ComputePipeline.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

#pragma region DX12ComputePipeline

DX12ComputePipeline::DX12ComputePipeline()
    : ComputePipelineBase()
{
}

DX12ComputePipeline::DX12ComputePipeline(const DX12ShaderRef& shader, const DX12DescriptorTableRef& descriptorTable)
    : ComputePipelineBase(shader, descriptorTable)
{
}

DX12ComputePipeline::~DX12ComputePipeline()
{
}

bool DX12ComputePipeline::IsCreated() const
{
    return false;
}

RendererResult DX12ComputePipeline::Create()
{
    // @TODO
    return {};
}

void DX12ComputePipeline::Bind(CommandBuffer* commandBuffer)
{
    // @TODO
}

void DX12ComputePipeline::Dispatch(CommandBuffer* commandBuffer, const Vec3u& groupSize) const
{
    // @TODO
}

void DX12ComputePipeline::DispatchIndirect(
    CommandBuffer* commandBuffer,
    const DX12GpuBufferRef& indirectBuffer,
    SizeType offset) const
{
    // @TODO
}

void DX12ComputePipeline::SetPushConstants(const void* data, SizeType size)
{
    // @TODO
}

#ifdef HYP_DEBUG_MODE
void DX12ComputePipeline::SetDebugName(Name name)
{
    ComputePipelineBase::SetDebugName(name);
}
#endif

#pragma endregion DX12ComputePipeline

} // namespace Hyperion
