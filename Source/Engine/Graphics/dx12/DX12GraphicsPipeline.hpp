/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/GraphicsPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/RenderPipeline.hpp>

#include <rendering/dx12/DX12Shared.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12GraphicsPipeline final : public GraphicsPipelineBase
{
    HYP_OBJECT_BODY(DX12GraphicsPipeline);

public:
    DX12GraphicsPipeline();
    explicit DX12GraphicsPipeline(const DX12ShaderInstanceRef& shaderInstance);
    ~DX12GraphicsPipeline() override;

    HYP_FORCE_INLINE ID3D12RootSignature* GetRootSignature() const
    {
        return m_rootSignature.Get();
    }

    HYP_FORCE_INLINE ID3D12PipelineState* GetPipelineState() const
    {
        return m_pipelineState.Get();
    }

    /*! \brief Get the root parameter indices for a descriptor set at the given bind index.
     *  \param bindIndex The descriptor set index (as used in Bind()). This corresponds to the HLSL register space.
     *  \return The root parameter indices for views and samplers.
     *
     *  @note The bindIndex corresponds to the descriptor set index which maps to HLSL register spaces.
     *  In ShaderCompiler.cpp, descriptor set indices are mapped to HLSL register spaces via
     *  #define _{SetName}_SPACE space{N} where N is the set index. The root signature is built
     *  with RegisterSpace = setIndex to match this mapping. */
    HYP_FORCE_INLINE const DescriptorSetRootIndices& GetDescriptorSetRootIndices(uint32 bindIndex) const
    {
        Assert(bindIndex < m_descriptorSetRootIndices.Size());
        return m_descriptorSetRootIndices[bindIndex];
    }

    bool IsCreated() const override;

    RendererResult Create() override;

    void Bind(DX12CommandBuffer* cmd) override;
    void Bind(DX12CommandBuffer* cmd, Vec2i viewportOffset, Vec2u viewportExtent) override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

    static bool CanDynamicallySetDepthState() { return false; }

private:
    RendererResult Rebuild() override;

    void BuildVertexAttributes(
        Array<D3D12_INPUT_ELEMENT_DESC>& outInputElementDescs,
        Array<uint32>& outBindingStrides);

    void UpdateViewport(DX12CommandBuffer* commandBuffer, const Viewport& viewport);

    RendererResult BuildRootSignature();

    Viewport m_viewport;

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    // Maps descriptor set index (bindIndex) to root parameter indices.
    // The descriptor set index corresponds to the HLSL register space (spaceN).
    // This mapping aligns with ShaderCompiler.cpp where #define _{SetName}_SPACE space{N} is generated.
    Array<DescriptorSetRootIndices, DX12Allocator> m_descriptorSetRootIndices;
};

} // namespace Hyperion
