#ifndef IMMUTABLE_STRING_HPP
#define IMMUTABLE_STRING_HPP

#include <core/lib/String.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

#include <cstring>

namespace hyperion {
namespace vm {

class VMString
{
public:
    static VMString Concat(const VMString &a, const VMString &b);

public:
    VMString(const char *str);
    VMString(const char *str, Int max_len);
    VMString(const String &str);
    VMString(String &&str);
    VMString(const VMString &other);
    VMString &operator=(const VMString &other);
    VMString(VMString &&other) noexcept;
    VMString &operator=(VMString &&other) noexcept;
    ~VMString();

    bool operator==(const VMString &other) const
        { return m_str == other.m_str; }

    const char *GetData() const
        { return m_str.Data(); }

    SizeType GetLength() const
        { return m_str.Size(); /* need to use size as other places are relying on it for memory size */ }

    const String &GetString() const
        { return m_str; }

    explicit operator String() const
        { return m_str; }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
        { return m_str.GetHashCode(); }

private:
    String  m_str;
};

} // namespace vm
} // namespace hyperion

#endif
