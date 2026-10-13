/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/RayTracingPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/WebGPU/WebGPUShared.hpp>

namespace Hyperion {

/// STUB ///
HYP_CLASS(NoScriptBindings)
class WebGPURayTracingPipeline final : public RayTracingPipelineBase
{
    HYP_OBJECT_BODY(WebGPURayTracingPipeline);

public:
    WebGPURayTracingPipeline();
    explicit WebGPURayTracingPipeline(const WebGPUShaderInstanceRef& shaderInstance);
    ~WebGPURayTracingPipeline() override;

    bool IsCreated() const override;
    RendererResult Create() override;

    void Bind(WebGPUCommandBuffer* commandBuffer) override;
    void TraceRays(WebGPUCommandBuffer* commandBuffer, const Vec3u& extent) const override;
};

} // namespace Hyperion
