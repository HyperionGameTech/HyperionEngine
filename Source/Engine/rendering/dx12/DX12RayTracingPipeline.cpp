/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12RayTracingPipeline.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>

#include <DX12RayTracingPipeline.generated.inl>

namespace Hyperion {

extern DX12RenderInterface RI;

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
    DX12CommandBuffer* dx12CommandBuffer = static_cast<DX12CommandBuffer*>(commandBuffer);
    Assert(dx12CommandBuffer != nullptr);

    dx12CommandBuffer->ResetBoundDescriptorSets();

    // @TODO
}

void DX12RayTracingPipeline::TraceRays(CommandBuffer* commandBuffer, const Vec3u& extent) const
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
