/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <rendering/dx12/DX12Shared.hpp>

namespace Hyperion {

enum class DX12DescriptorHeapType : uint32
{
    CBV_SRV_UAV,
    SAMPLER,
    RTV,
    DSV,

    MAX
};

struct DX12DescriptorHandle
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
    uint32 heapIndex = 0;
    uint32 count = 0;
    
    bool IsValid() const
    {
        return count != 0;
    }
};

class DX12DescriptorAllocator final
{
public:
    DX12DescriptorAllocator() = default;

    void Initialize(DX12DescriptorHeapType heapType, ID3D12DescriptorHeap* descriptorHeap, uint32 descriptorSize);

    DX12DescriptorHandle Allocate(uint32 count);
    void Free(const DX12DescriptorHandle& handle);

private:
    
};

class DX12DescriptorHeapManager final
{
public:
    DX12DescriptorHeapManager();

    DX12DescriptorHeapManager(const DX12DescriptorHeapManager&) = delete;
    DX12DescriptorHeapManager& operator=(const DX12DescriptorHeapManager&) = delete;

    DX12DescriptorHeapManager(DX12DescriptorHeapManager&&) = delete;
    DX12DescriptorHeapManager& operator=(DX12DescriptorHeapManager&&) = delete;

    void Initialize();
    void Shutdown();

    uint32 GetDescriptorSize(DX12DescriptorHeapType heapType) const;

    ID3D12DescriptorHeap* GetDescriptorHeap(DX12DescriptorHeapType heapType) const;

private:
    uint32 m_descriptorSizes[uint32(DX12DescriptorHeapType::MAX)];
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeaps[uint32(DX12DescriptorHeapType::MAX)];
};

} // namespace Hyperion
