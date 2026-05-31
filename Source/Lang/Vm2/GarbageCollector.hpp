#pragma once

#include <Lang/vm/Value.hpp>
#include <Lang/vm/ScriptMemory.hpp>

#include <Core/Types.hpp>

#include <Core/reflection/BoxedValue.hpp>

#include <Core/utilities/IdGenerator.hpp>

#include <Core/containers/Bitset.hpp>
#include <Core/containers/SparsePagedArray.hpp>

#include <Core/utilities/Span.hpp>
#include <Core/utilities/ValueStorage.hpp>

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
