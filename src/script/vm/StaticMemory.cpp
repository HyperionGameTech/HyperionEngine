#include <script/vm/StaticMemory.hpp>
#include <script/vm/HeapValue.hpp>

namespace hyperion {
namespace vm {

const UInt16 StaticMemory::static_size = 1000;

StaticMemory::StaticMemory()
    : m_data(new Value[static_size]),
      m_sp(0)
{
}

StaticMemory::~StaticMemory()
{
    // purge the items that are owned by this object
    Purge();
    // lastly delete the array
    delete[] m_data;
}

void StaticMemory::Purge()
{
    // delete all objects that are heap allocated
    for (; m_sp; m_sp--) {
        Value &sv = m_data[m_sp - 1];
        if (sv.m_type == Value::HEAP_POINTER && sv.m_value.ptr != nullptr) {
            delete sv.m_value.ptr;
        }
    }
}

} // namespace vm
} // namespace hyperion
