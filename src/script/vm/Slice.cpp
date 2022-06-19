#include <script/vm/Slice.hpp>

#include <system/debug.h>

#include <sstream>

namespace hyperion {
namespace vm {

Slice::Slice(Array *ary, SizeType start, SizeType end)
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
    if (&other == this) {
        return *this;
    }

    m_ary = other.m_ary;
    m_start = other.m_start;
    m_end = other.m_end;

    AssertThrow(m_ary != nullptr);
    AssertThrow(m_end >= m_start);

    return *this;
}

void Slice::GetRepresentation(
    std::stringstream &ss,
    bool add_type_name,
    int depth
) const
{
    AssertThrow(m_ary != nullptr);

    if (depth == 0) {
        ss << "[...]";

        return;
    }

    // convert array list to string
    const char sep_str[3] = ", ";

    ss << '[';

    // convert all array elements to string
    for (SizeType i = m_start; i < m_end; i++) {
        m_ary->AtIndex(i).ToRepresentation(
            ss,
            add_type_name,
            depth - 1
        );

        if (i != m_end - 1) {
            ss << sep_str;
        }
    }

    ss << ']';
}

}
}
