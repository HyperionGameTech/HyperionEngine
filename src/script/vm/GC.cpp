#include <script/vm/GC.hpp>

namespace hyperion {

GC::GC()
    : m_pool(NAME("GCMemoryPool"))
{
}

GC::~GC()
{
    // clear all allocated memory
    /// @TODO: Need to set GC index of all Values to INVALID_GC_INDEX then destruct
}

Value* GC::MoveToTrackedMemory(Value&& value)
{
    Assert(value.m_gcIndex == INVALID_GC_INDEX);
    Assert(!value.IsRef());

    Value* ptr;
    uint32 gcIndex = m_pool.AcquireIndex(&ptr) + 1; // reserve 0 for INVALID_GC_INDEX

    *ptr = std::move(value);
    ptr->m_gcIndex = GCIndex(gcIndex);

    return ptr;
}

} // namespace hyperion
