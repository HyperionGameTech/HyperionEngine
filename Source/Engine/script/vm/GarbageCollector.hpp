#pragma once

#include <script/vm/Value.hpp>

#include <Core/Types.hpp>

#include <Core/memory/allocator/SlabAllocator.hpp>

#include <Core/utilities/IdGenerator.hpp>

namespace Hyperion {

class GarbageCollector
{
public:
    GarbageCollector();

    GarbageCollector(const GarbageCollector& other) = delete;
    GarbageCollector& operator=(const GarbageCollector& other) = delete;

    GarbageCollector(GarbageCollector&& other) = delete;
    GarbageCollector& operator=(GarbageCollector&& other) = delete;

    ~GarbageCollector();

    void MoveToTrackedMemory(BoxedValue& inOutRefValue);

private:
    SlabAllocator m_allocator;
    IdGenerator m_idGenerator;
};

} // namespace Hyperion
