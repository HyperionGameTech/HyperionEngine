/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/GraphicsPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/WebGPU/WebGPUShared.hpp>
#include <Rendering/WebGPU/WebGPUShaderInstance.hpp>
#include <Rendering/WebGPU/WebGPUPipelineCompiler.hpp>

namespace Hyperion {

// A WebGPU pipeline is tied to exact bind group layouts, which depend on what is bound (a filterable texture or not,
// a filtering sampler or not), so the pipeline object is built per layout combination the first time it is drawn with.
HYP_CLASS(NoScriptBindings)
class WebGPUGraphicsPipeline final : public GraphicsPipelineBase
{
    HYP_OBJECT_BODY(WebGPUGraphicsPipeline);

public:
    WebGPUGraphicsPipeline();
    explicit WebGPUGraphicsPipeline(const WebGPUShaderInstanceRef& shaderInstance);
    ~WebGPUGraphicsPipeline() override;

    HYP_FORCE_INLINE uint32 GetNumBindGroups() const
    {
        return m_shaderInstance->GetNumBindGroups();
    }

    HYP_FORCE_INLINE bool UsesBindGroup(uint32 bindIndex) const
    {
        return m_shaderInstance->GetReflectedGroup(bindIndex).bindings.Any();
    }

    WGPURenderPipeline GetOrCreateVariant(const WGPUBindGroupLayout* layouts, uint32 numLayouts);

    bool IsCreated() const override;
    RendererResult Create() override;

    void Bind(WebGPUCommandBuffer* commandBuffer) override;
    void Bind(WebGPUCommandBuffer* commandBuffer, Vec2i viewportOffset, Vec2u viewportExtent) override;

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override;
#endif

    static bool CanDynamicallySetDepthState()
    {
        return false;
    }

    static bool CanDynamicallySetStencilMasks()
    {
        return false;
    }

private:
    using PipelineVariant = WebGPUPipelineVariant<WGPURenderPipeline>;

    static void OnVariantCompiled(WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, WGPUStringView message, void* userdata1, void* userdata2);

    RendererResult Rebuild() override;

    void ReleaseVariants();

    Array<PipelineVariant*, WebGPUAllocator> m_variants;
    bool m_isCreated;
};

} // namespace Hyperion
