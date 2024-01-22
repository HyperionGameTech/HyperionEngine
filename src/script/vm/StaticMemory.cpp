#include <script/vm/StaticMemory.hpp>
#include <script/vm/HeapValue.hpp>

namespace hyperion {
namespace vm {

const UInt16 StaticMemory::static_size = 65535;

StaticMemory::StaticMemory()
    : m_data(new Value[static_size])
{
    Memory::MemSet(m_data, 0, static_size * sizeof(Value));
}

StaticMemory::~StaticMemory()
{
    // purge the items that are owned by this object
    MarkAllForDeallocation();
    // lastly delete the array
    delete[] m_data;
}

void StaticMemory::MarkAllForDeallocation()
{
    // delete all objects that are heap allocated
    for (UInt32 i = static_size; i != 0; i--) {
        Value &sv = m_data[i - 1];

        if (sv.m_type == Value::HEAP_POINTER && sv.m_value.ptr != nullptr) {
            sv.m_value.ptr->DisableFlags(GC_ALWAYS_ALIVE);
        }
    }
}

} // namespace vm
} // namespace hyperion
