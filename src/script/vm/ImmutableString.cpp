#include <script/vm/ImmutableString.hpp>
#include <iostream>
namespace hyperion {
namespace vm {
  
ImmutableString::ImmutableString(const char *str)
{
    size_t len = std::strlen(str);
    m_length = len;

    m_data = new char[len + 1];
    std::strcpy(m_data, str);
    m_data[len] = '\0';
}
  
ImmutableString::ImmutableString(const char *str, size_t len)
{
    m_length = len;
    m_data = new char[len + 1];
    std::strcpy(m_data, str);
    m_data[len] = '\0';
}

ImmutableString::ImmutableString(const ImmutableString &other)
{
    m_length = other.m_length;
    m_data = new char[m_length + 1];
    std::strcpy(m_data, other.m_data);
    m_data[m_length] = '\0';
}


ImmutableString &ImmutableString::operator=(const ImmutableString &other)
{
    if (m_data) {
        delete[] m_data;
        m_data = nullptr;
        m_length = 0;
    }

    if (other.m_data) {
        m_length = other.m_length;
        m_data = new char[m_length + 1];
        std::strcpy(m_data, other.m_data);
        m_data[m_length] = '\0';
    }

    return *this;
}

ImmutableString::ImmutableString(ImmutableString &&other) noexcept
{
    m_data = other.m_data;
    m_length = other.m_length;

    other.m_data = nullptr;
    other.m_length = 0;
}

ImmutableString &ImmutableString::operator=(ImmutableString &&other) noexcept
{
    if (m_data) {
        delete[] m_data;
        m_data = nullptr;
        m_length = 0;
    }

    m_data = other.m_data;
    m_length = other.m_length;

    other.m_data = nullptr;
    other.m_length = 0;

    return *this;
}

ImmutableString::~ImmutableString()
{
    if (m_data) {
        delete[] m_data;
    }
}

ImmutableString ImmutableString::Concat(const ImmutableString &a, const ImmutableString &b)
{
    const size_t a_len = a.GetLength();
    const size_t b_len = b.GetLength();

    const size_t len = a_len + b_len;
    char *str = new char[len + 1];

    std::memcpy(str, a.GetData(), a_len);
    std::memcpy(&str[a_len], b.GetData(), b_len);

    ImmutableString res(str, len);

    delete[] str;

    return res;
}

} // namespace vm
} // namespace hyperion
