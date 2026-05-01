/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12DescriptorHeaps.hpp>
#include <rendering/dx12/DX12GpuBuffer.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12Frame.hpp>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

#pragma region DX12DescriptorAllocator

DX12DescriptorAllocator::DX12DescriptorAllocator(DX12DescriptorHeapType type, uint32 descriptorSize)
    : type(type),
      incrementSize(0),
      cpuStart {},
      gpuStart {},
      m_indexAllocator { descriptorSize }
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc {};
    heapDesc.NumDescriptors = descriptorSize;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    switch (type)
    {
    case DX12DescriptorHeapType::CBV_SRV_UAV:
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags |= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        break;
    case DX12DescriptorHeapType::SAMPLER:
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        heapDesc.Flags |= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        break;
    case DX12DescriptorHeapType::RTV:
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        break;
    case DX12DescriptorHeapType::DSV:
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        break;
    default:
        HYP_UNREACHABLE();
    }

    HRESULT res = g_renderInterface->GetDevice()->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap), &heap);
    Assert(SUCCEEDED(res), "Failed to create descriptor heap! Error code: {}", res);
    
    incrementSize = g_renderInterface->GetDevice()->GetDescriptorHandleIncrementSize(heapDesc.Type);

    cpuStart = heap->GetCPUDescriptorHandleForHeapStart();

    if (heapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        gpuStart = heap->GetGPUDescriptorHandleForHeapStart();
    }
}

DX12DescriptorAllocator::~DX12DescriptorAllocator() = default;

DX12DescriptorHandle DX12DescriptorAllocator::Allocate(uint32 count)
{
    uint32 allocationOffset = m_indexAllocator.Allocate(count);

    if (allocationOffset == DX12DescriptorIndexAllocator::InvalidIndex)
        return DX12DescriptorHandle();

    DX12DescriptorHandle descriptorHandle;
    descriptorHandle.count = count;
    descriptorHandle.cpuHandle = { cpuStart.ptr + (incrementSize * allocationOffset) };

    if (gpuStart.ptr != 0)
        descriptorHandle.gpuHandle = { gpuStart.ptr + (incrementSize * allocationOffset) };
    else
        descriptorHandle.gpuHandle = { 0 };

    return descriptorHandle;
}

void DX12DescriptorAllocator::Free(DX12DescriptorHandle&& handle)
{
    if (!handle.IsValid())
        return;

    ptrdiff_t startIndex = (handle.cpuHandle.ptr - cpuStart.ptr) / incrementSize;
    Assert(startIndex >= 0 && startIndex + handle.count <= m_indexAllocator.maxSize);

    m_indexAllocator.Free(uint32(startIndex), handle.count);

    handle = {};
}

#pragma endregion DX12DescriptorAllocator

#pragma region DX12DescriptorHeapManager

DX12DescriptorHeapManager::DX12DescriptorHeapManager()
    : m_descriptorAllocators {}
{
}

void DX12DescriptorHeapManager::Initialize()
{
    ID3D12Device* device = g_renderInterface->GetDevice();

    // placeholder
    static constexpr uint32 MaxDescriptorsByHeapType[MaxDescriptorHeapType] = {
        65536,  // CBV_SRV_UAV
        2048,   // SAMPLER
        16384,  // RTV
        16384   // DSV
    };

    for (uint32 heapIndex = 0; heapIndex < MaxDescriptorHeapType; heapIndex++)
    {
        m_descriptorAllocators[heapIndex] = PoolNew<DX12DescriptorAllocator>(*g_renderPool, DX12DescriptorHeapType(heapIndex), MaxDescriptorsByHeapType[heapIndex]);
    }
}

void DX12DescriptorHeapManager::Shutdown()
{
    for (uint32 heapIndex = 0; heapIndex < MaxDescriptorHeapType; heapIndex++)
    {
        PoolDelete<DX12DescriptorAllocator>(*g_renderPool, m_descriptorAllocators[heapIndex]);
        m_descriptorAllocators[heapIndex] = nullptr;
    }
}

HYP_NODISCARD DX12DescriptorHandle DX12DescriptorHeapManager::Allocate(DX12DescriptorHeapType heapType, uint32 count)
{
    return m_descriptorAllocators[uint32(heapType)]->Allocate(count);
}

void DX12DescriptorHeapManager::Free(DX12DescriptorHeapType heapType, DX12DescriptorHandle&& handle)
{
    m_descriptorAllocators[uint32(heapType)]->Free(std::move(handle));
}

ID3D12DescriptorHeap* DX12DescriptorHeapManager::GetDescriptorHeap(DX12DescriptorHeapType heapType) const
{
    return m_descriptorAllocators[uint32(heapType)]->heap.Get();
}

#pragma endregion DX12DescriptorHeapManager

} // namespace Hyperion
