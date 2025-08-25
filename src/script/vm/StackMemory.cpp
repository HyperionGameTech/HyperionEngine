#include <script/vm/StackMemory.hpp>

#include <util/UTF8.hpp>

#include <iomanip>
#include <sstream>

namespace hyperion {
namespace vm {

std::ostream& operator<<(std::ostream& os, const Script_StackMemory& stack)
{
    // print table header
    os << std::left;
    os << std::setw(5) << "Index" << "| ";
    os << std::setw(18) << "Type" << "| ";
    os << std::setw(16) << "Value";
    os << std::endl;

    for (SizeType i = 0; i < stack.m_sp; i++)
    {
        const Value& value = stack.m_data[i].Get();

        os << std::setw(5) << i << "| ";

        os << std::setw(18);
        os << value.GetTypeString() << "| ";

        os << std::setw(16);

        std::stringstream ss;
        value.ToRepresentation(ss, false);
        os << ss.rdbuf();

        os << std::endl;
    }
    return os;
}

Script_StackMemory::Script_StackMemory()
    : m_sp(0)
{
}

Script_StackMemory::~Script_StackMemory() = default;

void Script_StackMemory::Purge()
{
    for (SizeType i = m_sp; i > 0; i--)
    {
        m_data[i - 1].Destruct();
    }

    m_sp = 0;
}

void Script_StackMemory::MarkAll()
{
    // for (SizeType i = 0; i < m_sp; i++)
    // {
    //     m_data[i].Mark();
    // }
}

} // namespace vm
} // namespace hyperion
