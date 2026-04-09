/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/DescriptorSet.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/RenderObject.hpp>

#include <rendering/dx12/DX12DescriptorHeaps.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12DescriptorSet final : public DescriptorSetBase
{
    HYP_OBJECT_BODY(DX12DescriptorSet);
    
    using ElementCache = HashMap<Name, Array<DX12DescriptorHandle>>;

public:
    explicit DX12DescriptorSet(const DescriptorSetLayout& layout);
    ~DX12DescriptorSet() override;

    bool IsCreated() const override;
    RendererResult Create() override;

    void UpdateDirtyState(bool* outIsDirty = nullptr) override;
    void Update(bool force = false) override;

    void Bind(CommandBuffer* commandBuffer, const GraphicsPipeline* pipeline, uint32 bindIndex) const override;
    void Bind(CommandBuffer* commandBuffer, const GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;

    void Bind(CommandBuffer* commandBuffer, const ComputePipeline* pipeline, uint32 bindIndex) const override;
    void Bind(CommandBuffer* commandBuffer, const ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;

    void Bind(CommandBuffer* commandBuffer, const RayTracingPipeline* pipeline, uint32 bindIndex) const override;
    void Bind(CommandBuffer* commandBuffer, const RayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;

    DescriptorSetRef Clone() const override;

    D3D12_CPU_DESCRIPTOR_HANDLE GetViewCpuHandle(uint32 binding) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetSamplerCpuHandle(uint32 binding) const;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    ElementCache m_elementCache;

    // Binding index -> heap offset (packed) for views (CBV/SRV/UAV)
    HashMap<uint32, uint32> m_viewBindingToHeapOffset;
    // Binding index -> heap offset (packed) for samplers
    HashMap<uint32, uint32> m_samplerBindingToHeapOffset;

    // Allocated descriptor handles
    DX12DescriptorHandle m_viewDescriptorHandle;
    DX12DescriptorHandle m_samplerDescriptorHandle;

    bool m_isCreated = false;
};

HYP_CLASS(NoScriptBindings)
class DX12DescriptorTable final : public DescriptorTableBase
{
    HYP_OBJECT_BODY(DX12DescriptorTable);

public:
    explicit DX12DescriptorTable(const ShaderInputGroup* decl)
        : DescriptorTableBase(decl)
    {
    }

    ~DX12DescriptorTable() override = default;
};

} // namespace Hyperion
