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

#include <Rendering/DX12/DX12Shared.hpp>
#include <Rendering/DX12/DX12GpuBuffer.hpp>

namespace Hyperion {

// DescriptorSetRootIndices is defined in DX12Shared.hpp

HYP_CLASS(NoScriptBindings)
class DX12RayTracingPipeline final : public RayTracingPipelineBase
{
    HYP_OBJECT_BODY(DX12RayTracingPipeline);

public:
    DX12RayTracingPipeline();
    DX12RayTracingPipeline(const DX12ShaderInstanceRef& shaderInstance);
    ~DX12RayTracingPipeline() override;

    /*! \brief Get the root parameter indices for a descriptor set at the given bind index.
     *  \param bindIndex The descriptor set index (as used in Bind()).
     *  \return The root parameter indices for views and samplers. */
    HYP_FORCE_INLINE const DescriptorSetRootIndices& GetDescriptorSetRootIndices(uint32 bindIndex) const
    {
        Assert(bindIndex < m_descriptorSetRootIndices.Size());
        return m_descriptorSetRootIndices[bindIndex];
    }

    HYP_FORCE_INLINE ID3D12StateObject* GetStateObject() const
    {
        return m_stateObject.Get();
    }

    bool IsCreated() const override;

    RendererResult Create() override;

    void Bind(CommandBuffer* commandBuffer) override;
    void TraceRays(CommandBuffer* commandBuffer, const Vec3u& extent) const override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    struct ShaderBindingTableEntry
    {
        DX12GpuBufferRef buffer;
        D3D12_GPU_VIRTUAL_ADDRESS address = 0;
        uint32 size = 0;
    };

    RendererResult BuildRootSignature();
    RendererResult BuildShaderBindingTables();

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12StateObject> m_stateObject;
    ComPtr<ID3D12StateObjectProperties> m_stateObjectProperties;

    ShaderBindingTableEntry m_rayGenShaderTable;
    ShaderBindingTableEntry m_missShaderTable;
    ShaderBindingTableEntry m_hitGroupShaderTable;

    // Maps descriptor set index (bindIndex) to root parameter indices
    Array<DescriptorSetRootIndices, DX12Allocator> m_descriptorSetRootIndices;
};

} // namespace Hyperion
