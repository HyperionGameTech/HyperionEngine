/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>
#include <core/Constants.hpp>

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

static constexpr uint32 MaxDescriptorHeapType = uint32(DX12DescriptorHeapType::MAX);

struct DX12DescriptorHandle
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
    uint32 count = 0;
    uint8 frameIndex = 0; // frame this was allocated on (0..NumFramesInFlight)
    
    bool IsValid() const
    {
        return count != 0;
    }
};

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

    ~DX12DescriptorIndexAllocator() = default;

    uint32 Allocate(uint32 count)
    {
        if (count == 0)
            return InvalidIndex;

        Mutex::Guard guard(m_mutex);

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

    void Free(uint32 startIndex, uint32 count)
    {
        if (startIndex == InvalidIndex)
            return;

        if (count == 0)
            return;

        Mutex::Guard guard(m_mutex);

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

    void Reset()
    {
        Mutex::Guard guard(m_mutex);
        idCounter = 0;
        m_numFreeIndices = 0;
        m_freeIndices.Clear();
    }

    uint32 maxSize;
    uint32 idCounter;

private:
    uint32 m_numFreeIndices;
    Bitset m_freeIndices;
    Mutex m_mutex;
};

class DX12DescriptorAllocator final
{
public:
    explicit DX12DescriptorAllocator(DX12DescriptorHeapType type, uint32 maxDescriptors);

    DX12DescriptorHandle Allocate(uint8 frameIndex, uint32 count);
    void Free(DX12DescriptorHandle&& handle);

    DX12DescriptorHeapType type;
    ComPtr<ID3D12DescriptorHeap> heap;
    uint32 incrementSize;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart;

private:
    Array<DX12DescriptorIndexAllocator, FixedAllocator<NumFramesInFlight>> m_indexAllocators;
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

    DX12DescriptorHandle Allocate(DX12DescriptorHeapType heapType, uint32 count);
    void Free(DX12DescriptorHeapType heapType, DX12DescriptorHandle&& handle);

    ID3D12DescriptorHeap* GetDescriptorHeap(DX12DescriptorHeapType heapType) const;

private:
    FixedArray<DX12DescriptorAllocator*, MaxDescriptorHeapType> m_descriptorAllocators;
};

} // namespace Hyperion
