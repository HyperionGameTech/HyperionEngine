#pragma once

#include <core/containers/String.hpp>
#include <core/Types.hpp>
#include <core/HashCode.hpp>

#include <cstring>

namespace hyperion {

class Script_String
{
public:
    static Script_String Concat(const Script_String& a, const Script_String& b);

public:
    explicit Script_String(const char* str);
    Script_String(const char* str, int maxLen);
    explicit Script_String(const String& str);
    explicit Script_String(String&& str);

    Script_String(const Script_String& other);
    Script_String& operator=(const Script_String& other);

    Script_String(Script_String&& other) noexcept;
    Script_String& operator=(Script_String&& other) noexcept;

    ~Script_String();

    bool operator==(const Script_String& other) const
    {
        return m_str == other.m_str;
    }

    bool operator!=(const Script_String& other) const
    {
        return m_str != other.m_str;
    }

    const char* GetData() const
    {
        return m_str.Data();
    }

    SizeType GetLength() const
    {
        return m_str.Size(); /* need to use size as other places are relying on it for memory size */
    }

    const String& GetString() const
    {
        return m_str;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return m_str.GetHashCode();
    }

private:
    String m_str;
};

} // namespace hyperion
