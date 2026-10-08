/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/ComputePipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/WebGPU/WebGPUShared.hpp>
#include <Rendering/WebGPU/WebGPUShaderInstance.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class WebGPUComputePipeline final : public ComputePipelineBase
{
    HYP_OBJECT_BODY(WebGPUComputePipeline);

public:
    WebGPUComputePipeline();
    explicit WebGPUComputePipeline(const WebGPUShaderInstanceRef& shaderInstance);
    ~WebGPUComputePipeline() override;

    HYP_FORCE_INLINE uint32 GetNumBindGroups() const
    {
        return m_shaderInstance->GetNumBindGroups();
    }

    HYP_FORCE_INLINE bool UsesBindGroup(uint32 bindIndex) const
    {
        return m_shaderInstance->GetReflectedGroup(bindIndex).bindings.Any();
    }

    WGPUComputePipeline GetOrCreateVariant(const WGPUBindGroupLayout* layouts, uint32 numLayouts);

    bool IsCreated() const override;
    RendererResult Create() override;

    void Bind(CommandBuffer* commandBuffer) override;

    void Dispatch(CommandBuffer* commandBuffer, const Vec3u& groupSize) const override;

    void DispatchIndirect(
        CommandBuffer* commandBuffer,
        const WebGPUGpuBufferRef& indirectBuffer,
        size_t offset = 0) const override;

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override;
#endif

private:
    struct Variant
    {
        uint64 key;
        WGPUComputePipeline pipeline;
    };

    Array<Variant, WebGPUAllocator> m_variants;
    bool m_isCreated;
};

} // namespace Hyperion
