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

#include <Core/name/Name.hpp>
#include <Core/utilities/Optional.hpp>
#include <Core/containers/ArrayMap.hpp>
#include <Core/Defines.hpp>

#include <rendering/RenderTypes.hpp>

#include <rendering/dx12/DX12DescriptorHeaps.hpp>

namespace Hyperion {

struct DX12CachedDescriptor
{
    uint32 binding;
    uint32 index;
    D3D12_DESCRIPTOR_HEAP_TYPE heapType;

    union
    {
        ObjectBase* objectPtr;
        uint64 deviceAddress;
    };

    bool operator==(const DX12CachedDescriptor& other) const
    {
        if (binding != other.binding
            || index != other.index
            || heapType != other.heapType)
        {
            return false;
        }

        return objectPtr == other.objectPtr;
    }

    HYP_FORCE_INLINE bool operator!=(const DX12CachedDescriptor& other) const
    {
        return !(*this == other);
    }
};

HYP_CLASS(NoScriptBindings)
class DX12DescriptorSet final : public DescriptorSetBase
{
    HYP_OBJECT_BODY(DX12DescriptorSet);

    using ElementCache = TMap<Name, Array<DX12CachedDescriptor, DX12Allocator>, DX12Allocator>;

public:
    explicit DX12DescriptorSet(const DescriptorSetLayout& layout);
    ~DX12DescriptorSet() override;

    bool IsCreated() const override;
    RendererResult Create() override;

    void UpdateDirtyState(bool* outIsDirty = nullptr) override;
    void Update(bool force = false) override;

    void Bind(DX12CommandBuffer* commandBuffer, const DX12GraphicsPipeline* pipeline, uint32 bindIndex) const override;
    void Bind(DX12CommandBuffer* commandBuffer, const DX12GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;

    void Bind(DX12CommandBuffer* commandBuffer, const DX12ComputePipeline* pipeline, uint32 bindIndex) const override;
    void Bind(DX12CommandBuffer* commandBuffer, const DX12ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;

    void Bind(DX12CommandBuffer* commandBuffer, const DX12RayTracingPipeline* pipeline, uint32 bindIndex) const override;
    void Bind(DX12CommandBuffer* commandBuffer, const DX12RayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;

    DX12DescriptorSetRef Clone() const override;

    D3D12_CPU_DESCRIPTOR_HANDLE GetViewCpuHandle(uint32 binding) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetSamplerCpuHandle(uint32 binding) const;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    ElementCache m_cachedElements;
    Array<DX12CachedDescriptor, RHIAllocator> m_pendingDescriptors;

    // Binding index -> heap offset (packed) for views (CBV/SRV/UAV)
    TMap<uint32, uint32, RHIAllocator> m_viewBindingToHeapOffset;
    // Binding index -> heap offset (packed) for samplers
    TMap<uint32, uint32, RHIAllocator> m_samplerBindingToHeapOffset;

    // Allocated descriptor handles
    DX12DescriptorHandle m_viewDescriptorHandle;
    DX12DescriptorHandle m_samplerDescriptorHandle;

    uint16 m_updateVersion;

    bool m_isCreated : 1;
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
