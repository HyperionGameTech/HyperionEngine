#include <script/vm/VMArraySlice.hpp>

#include <system/Debug.hpp>

#include <sstream>

namespace hyperion {
namespace vm {

VMArraySlice::VMArraySlice(VMArray *ary, SizeType start, SizeType end)
    : m_ary(ary),
      m_start(start),
      m_end(end)
{
    AssertThrow(m_ary != nullptr);
    AssertThrow(m_end >= m_start);
}

VMArraySlice::VMArraySlice(const VMArraySlice &other)
    : m_ary(other.m_ary),
      m_start(other.m_start),
      m_end(other.m_end)
{
    AssertThrow(m_ary != nullptr);
    AssertThrow(m_end >= m_start);
}

VMArraySlice &VMArraySlice::operator=(const VMArraySlice &other)
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

void VMArraySlice::GetRepresentation(
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

HashCode VMArraySlice::GetHashCode() const
{
    AssertThrow(m_ary != nullptr);

    HashCode hash_code = 0;

    for (SizeType i = m_start; i < m_end; i++) {
        hash_code.Add(m_ary->AtIndex(i).GetHashCode());
    }

    return hash_code;
}

} // namespace vm
} // namespace hyperion
