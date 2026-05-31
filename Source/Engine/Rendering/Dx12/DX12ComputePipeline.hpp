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

#include <Rendering/DX12/DX12Shared.hpp>

namespace Hyperion {

// DescriptorSetRootIndices is defined in DX12Shared.hpp

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

    /*! \brief Get the root parameter indices for a descriptor set at the given bind index.
     *  \param bindIndex The descriptor set index (as used in Bind()).
     *  \return The root parameter indices for views and samplers. */
    HYP_FORCE_INLINE const DescriptorSetRootIndices& GetDescriptorSetRootIndices(uint32 bindIndex) const
    {
        Assert(bindIndex < m_descriptorSetRootIndices.Size());
        return m_descriptorSetRootIndices[bindIndex];
    }

    bool IsCreated() const override;

    RendererResult Create() override;

    void Bind(CommandBuffer* commandBuffer) override;

    void Dispatch(CommandBuffer* commandBuffer, const Vec3u& groupSize) const override;
    void DispatchIndirect(
        CommandBuffer* commandBuffer,
        const DX12GpuBufferRef& indirectBuffer,
        size_t offset = 0) const override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    RendererResult BuildRootSignature();

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    // Maps descriptor set index (bindIndex) to root parameter indices
    Array<DescriptorSetRootIndices> m_descriptorSetRootIndices;

    // Lazy-created command signature for DispatchIndirect
    mutable ComPtr<ID3D12CommandSignature> m_dispatchCommandSignature;
};

} // namespace Hyperion
