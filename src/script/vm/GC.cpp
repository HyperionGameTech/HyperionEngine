#include <script/vm/GC.hpp>

namespace hyperion {

Script_GC::Script_GC()
    : m_pool(NAME("GCMemoryPool"))
{
}

Script_GC::~Script_GC()
{
    // clear all allocated memory
    /// @TODO: Need to set Script_GC index of all Values to INVALID_GC_INDEX then destruct
}

Script_Value* Script_GC::MoveToTrackedMemory(Script_Value&& value)
{
    Assert(value.m_gcIndex == INVALID_GC_INDEX);
    Assert(!value.IsRef());

    Script_Value* ptr;
    uint32 gcIndex = m_pool.AcquireIndex(&ptr) + 1; // reserve 0 for INVALID_GC_INDEX

    *ptr = std::move(value);
    ptr->m_gcIndex = GCIndex(gcIndex);

    return ptr;
}

} // namespace hyperion
