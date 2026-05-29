/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>
#include <Core/Constants.hpp>

#include <Core/threading/SharedMutex.hpp>

#include <rendering/RenderMemory.hpp>

#include <rendering/dx12/DX12Shared.hpp>

namespace Hyperion {

enum class DX12DescriptorHeapType : uint8
{
    CBV_SRV_UAV,
    SAMPLER,
    RTV,
    DSV,

    MAX
};

static constexpr uint8 MaxDescriptorHeapType = uint8(DX12DescriptorHeapType::MAX);

struct DX12DescriptorHandle
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle { 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle { 0 };
    uint32 count = 0;
    
    bool IsValid() const
    {
        return count != 0;
    }
};

class DX12DescriptorAllocator;

class DX12DescriptorHeapManager final
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_dx12Pool);

    DX12DescriptorHeapManager();

    DX12DescriptorHeapManager(const DX12DescriptorHeapManager&) = delete;
    DX12DescriptorHeapManager& operator=(const DX12DescriptorHeapManager&) = delete;

    DX12DescriptorHeapManager(DX12DescriptorHeapManager&&) = delete;
    DX12DescriptorHeapManager& operator=(DX12DescriptorHeapManager&&) = delete;

    void Initialize();
    void Shutdown();

    DX12DescriptorHandle Allocate(DX12DescriptorHeapType heapType, uint32 count);
    void Free(DX12DescriptorHeapType heapType, DX12DescriptorHandle&& handle);

    bool CheckIsValidDescriptor(DX12DescriptorHeapType heapType, const DX12DescriptorHandle& handle) const;

    ID3D12DescriptorHeap* GetDescriptorHeap(DX12DescriptorHeapType heapType) const;

private:
    FixedArray<DX12DescriptorAllocator*, MaxDescriptorHeapType> m_descriptorAllocators;
};

} // namespace Hyperion
