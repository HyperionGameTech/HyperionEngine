#pragma once

#include <Lang/VM/Value.hpp>
#include <Lang/VM/ScriptMemory.hpp>

#include <Core/Types.hpp>

#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Utilities/IdGenerator.hpp>

#include <Core/Containers/Bitset.hpp>
#include <Core/Containers/SparsePagedArray.hpp>

#include <Core/Utilities/Span.hpp>
#include <Core/Utilities/ValueStorage.hpp>

namespace Hyperion {

class GarbageCollector
{
    static constexpr uint32 TrackedPageSize = 64;

public:
    GarbageCollector();

    GarbageCollector(const GarbageCollector& other) = delete;
    GarbageCollector& operator=(const GarbageCollector& other) = delete;

    GarbageCollector(GarbageCollector&& other) = delete;
    GarbageCollector& operator=(GarbageCollector&& other) = delete;

    ~GarbageCollector();

    void MoveToTrackedMemory(BoxedValue& inOutRefValue);

    void ClearMarks();
    void MarkReachable(Span<BoxedValue> values);
    void Collect();

    void Collect(Span<BoxedValue> roots);

private:
    void MarkReachable(BoxedValue& value);

    IdGenerator m_idGenerator;

    using TrackedStorage = ValueStorage<BoxedValue>;
    SparsePagedArray<TrackedStorage, TrackedPageSize, ScriptAllocator> m_trackedObjects;
    TBitset<ScriptAllocator> m_marks;
};

} // namespace Hyperion
