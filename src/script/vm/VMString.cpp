#include <script/vm/VMString.hpp>
#include <iostream>
namespace hyperion {
namespace vm {
  
VMString::VMString(const char *str)
    : m_str(str)
{
}
  
VMString::VMString(const char *str, Int max_len)
    : m_str(str, max_len)
{
}

VMString::VMString(const String &str)
    : m_str(str)
{
}

VMString::VMString(String &&str)
    : m_str(std::move(str))
{
}

VMString::VMString(const VMString &other)
    : m_str(other.m_str)
{
}


VMString &VMString::operator=(const VMString &other)
{
    if (std::addressof(other) == this) {
        return *this;
    }

    m_str = other.m_str;

    return *this;
}

VMString::VMString(VMString &&other) noexcept
    : m_str(std::move(other.m_str))
{
}

VMString &VMString::operator=(VMString &&other) noexcept
{
    if (std::addressof(other) == this) {
        return *this;
    }

    m_str = std::move(other.m_str);

    return *this;
}

VMString::~VMString() = default;

VMString VMString::Concat(const VMString &a, const VMString &b)
{
    return a.GetString() + b.GetString();
}

} // namespace vm
} // namespace hyperion
