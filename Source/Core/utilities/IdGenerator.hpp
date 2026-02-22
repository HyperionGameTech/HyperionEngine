/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>

#include <core/containers/Queue.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/containers/Bitset.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>

#include <mutex>
#include <atomic>

namespace Hyperion {

struct IdGenerator
{
    AtomicVar<uint32> idCounter;
    AtomicVar<uint32> numFreeIndices;
    Bitset freeIndices;
    Mutex freeIdMutex;

    IdGenerator()
        : idCounter(0),
          numFreeIndices(0)
    {
    }

    IdGenerator(const IdGenerator&) = delete;
    IdGenerator& operator=(const IdGenerator&) = delete;

    IdGenerator(IdGenerator&& other) noexcept
        : idCounter(other.idCounter.Exchange(0, MemoryOrder::ACQUIRE)),
          numFreeIndices(other.numFreeIndices.Exchange(0, MemoryOrder::ACQUIRE)),
          freeIndices(std::move(other.freeIndices))
    {
    }

    IdGenerator& operator=(IdGenerator&& other) noexcept = delete;

    ~IdGenerator() = default;

    uint32 Next()
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

                const uint32 index = bitIndex + 1;

                numFreeIndices.Decrement(1, MemoryOrder::RELEASE);

                return index;
            }
        }

        return idCounter.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1;
    }

    void ReleaseId(uint32 index)
    {
        HYP_CORE_ASSERT(index != 0, "Invalid index");

        Mutex::Guard guard(freeIdMutex);

        HYP_CORE_ASSERT(!freeIndices.Test(index - 1));

        freeIndices.Set(index - 1, true);
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
