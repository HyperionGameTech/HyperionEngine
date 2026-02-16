/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12RayTracingPipeline.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>

#include <DX12RayTracingPipeline.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

#pragma region DX12RayTracingPipeline

DX12RayTracingPipeline::DX12RayTracingPipeline()
    : RayTracingPipelineBase()
{
}

DX12RayTracingPipeline::DX12RayTracingPipeline(const DX12ShaderInstanceRef& shaderInstance)
    : RayTracingPipelineBase(shaderInstance)
{
}

DX12RayTracingPipeline::~DX12RayTracingPipeline()
{
}

bool DX12RayTracingPipeline::IsCreated() const
{
    return false;
}

RendererResult DX12RayTracingPipeline::Create()
{
    // @TODO
    return {};
}

void DX12RayTracingPipeline::Bind(CommandBuffer* commandBuffer)
{
    // @TODO
}

void DX12RayTracingPipeline::TraceRays(CommandBuffer* commandBuffer, const Vec3u& extent) const
{
    // @TODO
}

void DX12RayTracingPipeline::SetPushConstants(const void* data, SizeType size)
{
    // @TODO
}

#ifdef HYP_DEBUG_MODE
void DX12RayTracingPipeline::SetDebugName(Name name)
{
    RayTracingPipelineBase::SetDebugName(name);
}
#endif

#pragma endregion DX12RayTracingPipeline

} // namespace Hyperion
