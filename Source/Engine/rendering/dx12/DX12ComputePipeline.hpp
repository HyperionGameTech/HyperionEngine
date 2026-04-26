/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/ComputePipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/dx12/DX12Shared.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12ComputePipeline final : public ComputePipelineBase
{
    HYP_OBJECT_BODY(DX12ComputePipeline);

public:
    DX12ComputePipeline();
    explicit DX12ComputePipeline(const DX12ShaderInstanceRef& shaderInstance);
    ~DX12ComputePipeline() override;

    HYP_FORCE_INLINE ID3D12RootSignature* GetRootSignature() const
    {
        return m_rootSignature.Get();
    }

    HYP_FORCE_INLINE ID3D12PipelineState* GetPipelineState() const
    {
        return m_pipelineState.Get();
    }

    bool IsCreated() const override;

    RendererResult Create() override;

    void Bind(CommandBuffer* commandBuffer) override;

    void Dispatch(CommandBuffer* commandBuffer, const Vec3u& groupSize) const override;
    void DispatchIndirect(
        CommandBuffer* commandBuffer,
        const DX12GpuBufferRef& indirectBuffer,
        size_t offset = 0) const override;

    void SetPushConstants(const void* data, size_t size) override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    RendererResult BuildRootSignature();

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
};

} // namespace Hyperion
