/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUGpuBuffer.hpp>
#include <Rendering/WebGPU/WebGPURayTracingPipeline.hpp>
#include <Rendering/WebGPU/WebGPUShaderInstance.hpp>

#include <WebGPURayTracingPipeline.generated.inl>

namespace Hyperion {

WebGPURayTracingPipeline::WebGPURayTracingPipeline()
    : RayTracingPipelineBase()
{
}

WebGPURayTracingPipeline::WebGPURayTracingPipeline(const WebGPUShaderInstanceRef& shaderInstance)
    : RayTracingPipelineBase(shaderInstance)
{
}

WebGPURayTracingPipeline::~WebGPURayTracingPipeline() = default;

bool WebGPURayTracingPipeline::IsCreated() const
{
    return false;
}

RendererResult WebGPURayTracingPipeline::Create()
{
    return HYP_MAKE_ERROR(RendererError, "Ray tracing is not supported on WebGPU");
}

void WebGPURayTracingPipeline::Bind(WebGPUCommandBuffer* commandBuffer)
{
}

void WebGPURayTracingPipeline::TraceRays(WebGPUCommandBuffer* commandBuffer, const Vec3u& extent) const
{
}

} // namespace Hyperion
