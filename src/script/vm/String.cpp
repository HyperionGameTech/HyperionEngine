#include <script/vm/String.hpp>
#include <iostream>
namespace hyperion {

Script_String::Script_String(const char* str)
    : m_str(str)
{
}

Script_String::Script_String(const char* str, int maxLen)
    : m_str(UTF8StringView(str, str + maxLen))
{
}

Script_String::Script_String(const String& str)
    : m_str(str)
{
}

Script_String::Script_String(String&& str)
    : m_str(std::move(str))
{
}

Script_String::Script_String(const Script_String& other)
    : m_str(other.m_str)
{
}

Script_String& Script_String::operator=(const Script_String& other)
{
    if (std::addressof(other) == this)
    {
        return *this;
    }

    m_str = other.m_str;

    return *this;
}

Script_String::Script_String(Script_String&& other) noexcept
    : m_str(std::move(other.m_str))
{
}

Script_String& Script_String::operator=(Script_String&& other) noexcept
{
    if (std::addressof(other) == this)
    {
        return *this;
    }

    m_str = std::move(other.m_str);

    return *this;
}

Script_String::~Script_String() = default;

Script_String Script_String::Concat(const Script_String& a, const Script_String& b)
{
    return Script_String(a.GetString() + b.GetString());
}

} // namespace hyperion
