/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_CORE_ID_CREATOR_HPP
#define HYPERION_CORE_ID_CREATOR_HPP

#include <core/containers/Queue.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/ID.hpp>
#include <Constants.hpp>
#include <Types.hpp>
#include <core/Defines.hpp>

#include <mutex>
#include <atomic>

namespace hyperion {

struct IDGenerator
{
    TypeID              type_id;
    AtomicVar<uint32>   id_counter { 0u };
    Mutex               free_id_mutex;
    Queue<uint>         free_indices;

    uint NextID()
    {
        Mutex::Guard guard(free_id_mutex);

        if (free_indices.Empty()) {
            return id_counter.Increment(1, MemoryOrder::SEQUENTIAL) + 1;
        }

        return free_indices.Pop();
    }

    void FreeID(uint index)
    {
        Mutex::Guard guard(free_id_mutex);

        free_indices.Push(index);
    }
};

} // namespace hyperion

#endif