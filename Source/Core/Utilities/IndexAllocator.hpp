/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/Mutex.hpp>

#include <Core/Containers/Queue.hpp>
#include <Core/Containers/TypeMap.hpp>
#include <Core/Containers/Bitset.hpp>

#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <mutex>
#include <atomic>

namespace Hyperion {

struct IndexAllocator
{
    static constexpr uint32 InvalidIndex = ~0u;

    uint32 idCounter;
    Bitset freeIndices;

    IndexAllocator()
        : idCounter(0)
    {
    }

    IndexAllocator(const IndexAllocator&) = delete;
    IndexAllocator& operator=(const IndexAllocator&) = delete;

    IndexAllocator(IndexAllocator&& other) noexcept
        : idCounter(other.idCounter),
          freeIndices(std::move(other.freeIndices))
    {
        other.idCounter = 0;
    }

    IndexAllocator& operator=(IndexAllocator&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        idCounter = other.idCounter;
        freeIndices = std::move(other.freeIndices);

        other.idCounter = 0;

        return *this;
    }

    ~IndexAllocator() = default;

    HYP_NODISCARD uint32 Allocate()
    {
        if (freeIndices.Count() != 0)
        {
            Bitset::BitIndex bitIndex = freeIndices.LastSetBitIndex();

            HYP_CORE_ASSERT(bitIndex != Bitset::NotFound);
            HYP_CORE_ASSERT(freeIndices.Test(bitIndex) == true);

            freeIndices.Set(bitIndex, false);

            const uint32 index = bitIndex;

            return index;
        }

        return idCounter++;
    }

    void Free(uint32 index)
    {
        HYP_CORE_ASSERT(index != InvalidIndex, "Invalid index");

        if (index == InvalidIndex)
        {
            return;
        }

        HYP_CORE_ASSERT(!freeIndices.Test(index));

        freeIndices.Set(index, true);
    }

    void Reset()
    {
        idCounter = 0;
        freeIndices.Clear();
    }
};

struct AtomicIndexAllocator
{
    static constexpr uint32 InvalidIndex = ~0u;

    AtomicVar<uint32> idCounter;
    AtomicVar<uint32> numFreeIndices;
    Bitset freeIndices;
    Mutex freeIdMutex;

    AtomicIndexAllocator()
        : idCounter(0),
          numFreeIndices(0)
    {
    }

    AtomicIndexAllocator(const AtomicIndexAllocator&) = delete;
    AtomicIndexAllocator& operator=(const AtomicIndexAllocator&) = delete;

    AtomicIndexAllocator(AtomicIndexAllocator&& other) noexcept
        : idCounter(other.idCounter.Exchange(0, MemoryOrder::ACQUIRE)),
          numFreeIndices(other.numFreeIndices.Exchange(0, MemoryOrder::ACQUIRE)),
          freeIndices(std::move(other.freeIndices))
    {
    }

    AtomicIndexAllocator& operator=(AtomicIndexAllocator&& other) noexcept = delete;

    ~AtomicIndexAllocator() = default;

    HYP_NODISCARD uint32 Allocate()
    {
        uint32 currentNumFreeIndices;

        if ((currentNumFreeIndices = numFreeIndices.Get(MemoryOrder::ACQUIRE)) != 0)
        {
            Mutex::Guard guard(freeIdMutex);

            // Check that it hasn't changed before the lock
            if (freeIndices.Count() != 0)
            {
                Bitset::BitIndex bitIndex = freeIndices.LastSetBitIndex();
                HYP_CORE_ASSERT(bitIndex != Bitset::NotFound);
                HYP_CORE_ASSERT(freeIndices.Test(bitIndex) == true);
                freeIndices.Set(bitIndex, false);

                const uint32 index = bitIndex;

                numFreeIndices.Decrement(1, MemoryOrder::RELEASE);

                return index;
            }
        }

        return idCounter.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
    }

    void Free(uint32 index)
    {
        HYP_CORE_ASSERT(index != InvalidIndex, "Invalid index");

        if (index == InvalidIndex)
        {
            return;
        }

        Mutex::Guard guard(freeIdMutex);

        HYP_CORE_ASSERT(!freeIndices.Test(index));

        freeIndices.Set(index, true);
        numFreeIndices.Increment(1, MemoryOrder::RELEASE);
    }

    void Reset()
    {
        Mutex::Guard guard(freeIdMutex);

        idCounter.Set(0, MemoryOrder::RELEASE);
        numFreeIndices.Set(0, MemoryOrder::RELEASE);
        freeIndices.Clear();
    }
};

} // namespace Hyperion
