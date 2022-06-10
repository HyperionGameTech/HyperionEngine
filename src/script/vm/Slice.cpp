#include <script/vm/Slice.hpp>

#include <system/debug.h>

#include <sstream>

namespace hyperion {
namespace vm {

Slice::Slice(Array *ary, std::size_t start, std::size_t end)
    : m_ary(ary),
      m_start(start),
      m_end(end)
{
    AssertThrow(m_ary != nullptr);
    AssertThrow(m_end >= m_start);
}

Slice::Slice(const Slice &other)
    : m_ary(other.m_ary),
      m_start(other.m_start),
      m_end(other.m_end)
{
    AssertThrow(m_ary != nullptr);
    AssertThrow(m_end >= m_start);
}

Slice &Slice::operator=(const Slice &other)
{
    m_ary = other.m_ary;
    m_start = other.m_start;
    m_end = other.m_end;

    AssertThrow(m_ary != nullptr);
    AssertThrow(m_end >= m_start);
}

void Slice::GetRepresentation(std::stringstream &ss, bool add_type_name) const
{
    AssertThrow(m_ary != nullptr);

    // convert array list to string
    const char sep_str[3] = ", ";

    ss << '[';

    // convert all array elements to string
    for (size_t i = m_start; i < m_end; i++) {
        m_ary->AtIndex(i).ToRepresentation(ss, add_type_name);

        if (i != m_end - 1) {
            ss << sep_str;
        }
    }

    ss << ']';
}

}
}
