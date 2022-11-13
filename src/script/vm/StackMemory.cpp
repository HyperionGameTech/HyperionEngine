#include <script/vm/StackMemory.hpp>

#include <util/UTF8.hpp>

#include <iomanip>
#include <sstream>

namespace hyperion {
namespace vm {

const SizeType StackMemory::STACK_SIZE = 20000;

std::ostream &operator<<(std::ostream &os, const StackMemory &stack)
{
    // print table header
    os << std::left;
    os << std::setw(5) << "Index" << "| ";
    os << std::setw(18) << "Type" << "| ";
    os << std::setw(16) << "Value";
    os << std::endl;

    for (int i = 0; i < stack.m_sp; i++) {
        Value &value = stack.m_data[i];
        
        os << std::setw(5) << i << "| ";

        if (value.GetType() == Value::ValueType::HEAP_POINTER) {
            //os << std::setw(17);
            os << std::setw(18) << value.GetTypeString() << "| ";
        } else {
            os << std::setw(18);
            os << value.GetTypeString() << "| ";
        }

        os << std::setw(16);

        std::stringstream ss;
        value.ToRepresentation(ss, false);
        os << ss.rdbuf();

        os << std::endl;
    }
    return os;
}

StackMemory::StackMemory()
    : m_data(new Value[STACK_SIZE]),
      m_sp(0)
{
}

StackMemory::~StackMemory()
{
    delete[] m_data;
}

void StackMemory::Purge()
{
    // just set stack pointer to zero
    // heap allocated objects are not owned,
    // so we don't have to delete them
    // they will be deleted either by the destructor of the `Heap` class,
    // or by the `Purge` function of the `Heap` class.
    m_sp = 0;
}

void StackMemory::MarkAll()
{
    for (Int64 i = static_cast<Int64>(m_sp) - 1; i >= 0; i--) {
        m_data[i].Mark();
    }
}

} // namespace vm
} // namespace hyperion
