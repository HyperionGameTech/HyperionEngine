/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/DescriptorSet.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Core/Name/Name.hpp>
#include <Core/Utilities/Optional.hpp>
#include <Core/Containers/ArrayMap.hpp>
#include <Core/Defines.hpp>

#include <Rendering/RenderTypes.hpp>

#include <Rendering/DX12/DX12DescriptorHeaps.hpp>

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

/// Built as a wrapper around the engine's DescriptorSetBase to allow DX12 descriptors be grouped and be used in a generic way,
//  to align with the Vulkan backend's VulkanDescriptorSet.
HYP_CLASS(NoScriptBindings)
class DX12DescriptorSet final : public DescriptorSetBase
{
    HYP_OBJECT_BODY(DX12DescriptorSet);

    using ElementCache = Map<Name, Array<DX12CachedDescriptor, DX12Allocator>, DX12Allocator>;

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

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override;
#endif

private:
    ElementCache m_cachedElements;
    Array<DX12CachedDescriptor, RHIAllocator> m_pendingDescriptors;

    // Below two are 'maps' just arrays where each index corresponds to a binding.
    //  - Binding -> heap offset for views (CBV/SRV/UAV)
    Array<uint32, RHIAllocator> m_viewBindingToHeapOffset;
    //  - Binding -> heap offset for samplers
    Array<uint32, RHIAllocator> m_samplerBindingToHeapOffset;

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
