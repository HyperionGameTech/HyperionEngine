/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/RayTracingPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12RayTracingPipeline final : public RayTracingPipelineBase
{
    HYP_OBJECT_BODY(DX12RayTracingPipeline);

public:
    DX12RayTracingPipeline();
    DX12RayTracingPipeline(const DX12ShaderInstanceRef& shaderInstance);
    ~DX12RayTracingPipeline() override;

    bool IsCreated() const override;

    RendererResult Create() override;

    void Bind(CommandBuffer* commandBuffer) override;
    void TraceRays(CommandBuffer* commandBuffer, const Vec3u& extent) const override;

    void SetPushConstants(const void* data, size_t size) override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif
};

} // namespace Hyperion
