#ifndef IMMUTABLE_STRING_HPP
#define IMMUTABLE_STRING_HPP

#include <cstring>

namespace hyperion {
namespace vm {

class ImmutableString {
public:
    static ImmutableString Concat(const ImmutableString &a, const ImmutableString &b);

public:
    ImmutableString(const char *str);
    ImmutableString(const char *str, size_t len);
    ImmutableString(const ImmutableString &other);
    ~ImmutableString();

    inline bool operator==(const ImmutableString &other) const
        { return m_length == other.m_length && !std::strcmp(m_data, other.m_data); }

    inline const char *GetData() const
        { return m_data; }
    inline size_t GetLength() const
        { return m_length; }

private:
    char *m_data;
    // cache length
    size_t m_length;
};

} // namespace vm
} // namespace hyperion

#endif
