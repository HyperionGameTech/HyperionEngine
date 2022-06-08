#include <script/vm/HeapValue.hpp>

namespace hyperion {
namespace vm {

HeapValue::HeapValue()
    : m_holder(nullptr),
      m_ptr(nullptr),
      m_flags(0)
{
}

HeapValue::~HeapValue()
{
    if (m_holder) {
        delete m_holder;
    }
}

} // namespace vm
} // namespace hyperion
