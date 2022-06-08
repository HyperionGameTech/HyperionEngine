#include <script/vm/TypeInfo.hpp>
#include <cstring>

namespace hyperion {
namespace vm {

TypeInfo::TypeInfo(const char *name,
    size_t size,
    char **names)
    : m_size(size),
      m_names(new char*[size])
{
    size_t name_len = std::strlen(name);
    m_name = new char[name_len + 1];
    m_name[name_len] = '\0';
    std::memcpy(m_name, name, name_len);

    // copy all names
    for (size_t i = 0; i < m_size; i++) {
        size_t len = std::strlen(names[i]);
        m_names[i] = new char[len + 1];
        m_names[i][len] = '\0';
        std::memcpy(m_names[i], names[i], len);
    }
}

TypeInfo::TypeInfo(const TypeInfo &other)
    : m_size(other.m_size),
      m_names(new char*[other.m_size])
{
    size_t name_len = std::strlen(other.m_name);
    m_name = new char[name_len + 1];
    m_name[name_len] = '\0';
    std::memcpy(m_name, other.m_name, name_len);

    // copy all names
    for (size_t i = 0; i < m_size; i++) {
        size_t len = std::strlen(other.m_names[i]);
        m_names[i] = new char[len + 1];
        m_names[i][len] = '\0';
        std::memcpy(m_names[i], other.m_names[i], len);
    }
}

TypeInfo &TypeInfo::operator=(const TypeInfo &other)
{
    delete[] m_name;

    for (size_t i = 0; i < m_size; i++) {
        delete[] m_names[i];
    }

    if (m_size != other.m_size) {
        delete[] m_names;
        m_names = new char*[other.m_size];
    }

    m_size = other.m_size;

    size_t name_len = std::strlen(other.m_name);
    m_name = new char[name_len + 1];
    m_name[name_len] = '\0';
    std::memcpy(m_name, other.m_name, name_len);

    // copy all names
    for (size_t i = 0; i < m_size; i++) {
        size_t len = std::strlen(other.m_names[i]);
        m_names[i] = new char[len + 1];
        m_names[i][len] = '\0';
        std::memcpy(m_names[i], other.m_names[i], len);
    }

    return *this;
}

TypeInfo::~TypeInfo()
{
    delete[] m_name;
    
    for (size_t i = 0; i < m_size; i++) {
        delete[] m_names[i];
    }
    delete[] m_names;
}

bool TypeInfo::operator==(const TypeInfo &other) const
{
    // first, compare sizes
    if (m_size != other.m_size) {
        return false;
    }

    // compare type name
    if (std::strcmp(m_name, other.m_name)) {
        return false;
    }

    // compare names
    for (size_t i = 0; i < m_size; i++) {
        if (std::strcmp(m_names[i], other.m_names[i])) {
            return false;
        }
    }

    return true;
}

} // namespace vm
} // namespace hyperion
