/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <Rendering/dx12/DX12DescriptorHeaps.hpp>
#include <Rendering/dx12/DX12GpuBuffer.hpp>
#include <Rendering/dx12/DX12GpuImage.hpp>
#include <Rendering/dx12/DX12RenderInterface.hpp>
#include <Rendering/dx12/DX12Frame.hpp>

namespace Hyperion {

extern DX12RenderInterface RI;

#pragma region DX12DescriptorIndexAllocator


class DX12DescriptorIndexAllocator
{
public:
    static constexpr uint32 InvalidIndex = ~0u;

    explicit DX12DescriptorIndexAllocator(uint32 maxSize)
        : maxSize(maxSize),
          idCounter(0),
          m_numFreeIndices(0)
    {
    }

    DX12DescriptorIndexAllocator(const DX12DescriptorIndexAllocator&) = delete;
    DX12DescriptorIndexAllocator& operator=(const DX12DescriptorIndexAllocator&) = delete;

    DX12DescriptorIndexAllocator(DX12DescriptorIndexAllocator&&) = delete;
    DX12DescriptorIndexAllocator& operator=(DX12DescriptorIndexAllocator&&) = delete;

    ~DX12DescriptorIndexAllocator() = default;

    bool ContainsRange(uint32 startIndex, uint32 count) const;

    uint32 Allocate(uint32 count);
    void Free(uint32 startIndex, uint32 count);

    void Reset();

    uint32 maxSize;
    uint32 idCounter;

private:
    uint32 m_numFreeIndices;

    SharedMutex m_mutex;

    TBitset<DX12Allocator> m_freeIndices;
};

bool DX12DescriptorIndexAllocator::ContainsRange(uint32 startIndex, uint32 count) const
{
    TSharedLock lock(m_mutex);

    if (count == 0)
        return false;

    if (startIndex + count > idCounter)
        return false;

    for (uint32 i = 0; i < count; ++i)
    {
        if (m_freeIndices.Test(startIndex + i))
            return false;
    }

    return true;
}

uint32 DX12DescriptorIndexAllocator::Allocate(uint32 count)
{
    if (count == 0)
        return InvalidIndex;

    TUniqueLock lock(m_mutex);

    if (m_numFreeIndices >= count)
    {
        // First fit
        uint32 consecutiveFound = 0;
        uint32 searchStart = 0;
        bool runStarted = false;

        for (uint32 i = 0; i < idCounter; ++i)
        {
            if (m_freeIndices.Test(i))
            {
                if (!runStarted)
                {
                    searchStart = i;
                    runStarted = true;
                }

                consecutiveFound++;

                if (consecutiveFound == count)
                {
                    for (uint32 k = 0; k < count; ++k)
                    {
                        m_freeIndices.Set(searchStart + k, false);
                    }

                    m_numFreeIndices -= count;
                    return searchStart;
                }
            }
            else
            {
                runStarted = false;
                consecutiveFound = 0;
            }
        }
    }

    // out of slots!
    if (idCounter + count > maxSize)
        return InvalidIndex;

    uint32 start = idCounter;
    idCounter += count;

    return start;
}

void DX12DescriptorIndexAllocator::Free(uint32 startIndex, uint32 count)
{
    if (startIndex == InvalidIndex)
        return;

    if (count == 0)
        return;

    TUniqueLock lock(m_mutex);

    AssertDebug(startIndex + count <= idCounter);

    // mark free
    for (uint32 i = 0; i < count; ++i)
    {
        uint32 idx = startIndex + i;

        AssertDebug(!m_freeIndices.Test(idx));

        m_freeIndices.Set(idx, true);
    }

    m_numFreeIndices += count;

    // shrink
    while (idCounter > 0 && m_freeIndices.Test(idCounter - 1))
    {
        m_freeIndices.Set(idCounter - 1, false);
        idCounter--;
        m_numFreeIndices--;
    }
}

void DX12DescriptorIndexAllocator::Reset()
{
    TUniqueLock lock(m_mutex);

    idCounter = 0;
    m_numFreeIndices = 0;
    m_freeIndices.Clear();
}

#pragma endregion DX12DescriptorIndexAllocator

#pragma region DX12DescriptorAllocator

class DX12DescriptorAllocator final
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_dx12Pool);

    explicit DX12DescriptorAllocator(DX12DescriptorHeapType type, uint32 maxDescriptors);

    DX12DescriptorAllocator(const DX12DescriptorAllocator&) = delete;
    DX12DescriptorAllocator& operator=(const DX12DescriptorAllocator&) = delete;

    DX12DescriptorAllocator(DX12DescriptorAllocator&&) = delete;
    DX12DescriptorAllocator& operator=(DX12DescriptorAllocator&&) = delete;

    ~DX12DescriptorAllocator();

    HYP_NODISCARD DX12DescriptorHandle Allocate(uint32 count);
    void Free(DX12DescriptorHandle&& handle);

    bool IsValidHandle(const DX12DescriptorHandle& handle) const;

    DX12DescriptorHeapType type;

    ComPtr<ID3D12DescriptorHeap> heap;

    uint32 incrementSize;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart;

private:
    DX12DescriptorIndexAllocator m_indexAllocator;
};

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

    HRESULT res = RI.GetDevice()->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap), &heap);
    Assert(SUCCEEDED(res), "Failed to create descriptor heap! Error code: {}", res);

    incrementSize = RI.GetDevice()->GetDescriptorHandleIncrementSize(heapDesc.Type);

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

bool DX12DescriptorAllocator::IsValidHandle(const DX12DescriptorHandle& handle) const
{
    if (!handle.IsValid())
    {
        return false;
    }

    ptrdiff_t startIndex = (handle.cpuHandle.ptr - cpuStart.ptr) / incrementSize;
    Assert(startIndex >= 0 && startIndex + handle.count <= m_indexAllocator.maxSize);

    return m_indexAllocator.ContainsRange(uint32(startIndex), handle.count);
}

#pragma endregion DX12DescriptorAllocator

#pragma region DX12DescriptorHeapManager

DX12DescriptorHeapManager::DX12DescriptorHeapManager()
    : m_descriptorAllocators {}
{
}

void DX12DescriptorHeapManager::Initialize()
{
    ID3D12Device* device = RI.GetDevice();

    // placeholder
    static constexpr uint32 MaxDescriptorsByHeapType[MaxDescriptorHeapType] = {
        65536,  // CBV_SRV_UAV
        2048,   // SAMPLER
        16384,  // RTV
        16384   // DSV
    };

    for (uint8 heapIndex = 0; heapIndex < MaxDescriptorHeapType; heapIndex++)
    {
        m_descriptorAllocators[heapIndex] = new DX12DescriptorAllocator(DX12DescriptorHeapType(heapIndex), MaxDescriptorsByHeapType[heapIndex]);
    }
}

void DX12DescriptorHeapManager::Shutdown()
{
    for (uint8 heapIndex = 0; heapIndex < MaxDescriptorHeapType; heapIndex++)
    {
        delete m_descriptorAllocators[heapIndex];
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

bool DX12DescriptorHeapManager::CheckIsValidDescriptor(DX12DescriptorHeapType heapType, const DX12DescriptorHandle& handle) const
{
    const DX12DescriptorAllocator* allocator = m_descriptorAllocators[uint32(heapType)];

    if (!allocator)
    {
        return false;
    }

    return allocator->IsValidHandle(handle);
}

ID3D12DescriptorHeap* DX12DescriptorHeapManager::GetDescriptorHeap(DX12DescriptorHeapType heapType) const
{
    return m_descriptorAllocators[uint32(heapType)]->heap.Get();
}

#pragma endregion DX12DescriptorHeapManager

} // namespace Hyperion
