/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12RaytracingPipeline.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>

#include <DX12RaytracingPipeline.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

#pragma region DX12RaytracingPipeline

DX12RaytracingPipeline::DX12RaytracingPipeline()
    : RaytracingPipelineBase()
{
}

DX12RaytracingPipeline::DX12RaytracingPipeline(const DX12ShaderRef& shader, const DX12DescriptorTableRef& descriptorTable)
    : RaytracingPipelineBase(shader, descriptorTable)
{
}

DX12RaytracingPipeline::~DX12RaytracingPipeline()
{
}

bool DX12RaytracingPipeline::IsCreated() const
{
    return false;
}

RendererResult DX12RaytracingPipeline::Create()
{
    // @TODO
    HYPERION_RETURN_OK;
}

void DX12RaytracingPipeline::Bind(CommandBuffer* commandBuffer)
{
    // @TODO
}

void DX12RaytracingPipeline::TraceRays(CommandBuffer* commandBuffer, const Vec3u& extent) const
{
    // @TODO
}

void DX12RaytracingPipeline::SetPushConstants(const void* data, SizeType size)
{
    // @TODO
}

#ifdef HYP_DEBUG_MODE
void DX12RaytracingPipeline::SetDebugName(Name name)
{
    RaytracingPipelineBase::SetDebugName(name);
}
#endif

#pragma endregion DX12RaytracingPipeline

} // namespace Hyperion
