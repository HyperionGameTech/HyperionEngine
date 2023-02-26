#include <script/vm/VMTypeInfo.hpp>
#include <core/Core.hpp>
#include <cstring>

namespace hyperion {
namespace vm {

VMTypeInfo::VMTypeInfo(
    const char *name,
    SizeType size,
    char **names
) : m_size(size),
    m_names(new char*[size])
{
    SizeType name_len = std::strlen(name);
    m_name = new char[name_len + 1];
    m_name[name_len] = '\0';
    Memory::MemCpy(m_name, name, name_len);

    // copy all names
    for (size_t i = 0; i < m_size; i++) {
        size_t len = std::strlen(names[i]);
        m_names[i] = new char[len + 1];
        m_names[i][len] = '\0';
        Memory::MemCpy(m_names[i], names[i], len);
    }
}

VMTypeInfo::VMTypeInfo(const VMTypeInfo &other)
    : m_size(other.m_size),
      m_names(new char*[other.m_size])
{
    SizeType name_len = std::strlen(other.m_name);
    m_name = new char[name_len + 1];
    m_name[name_len] = '\0';
    Memory::MemCpy(m_name, other.m_name, name_len);

    // copy all names
    for (SizeType i = 0; i < m_size; i++) {
        SizeType len = std::strlen(other.m_names[i]);
        m_names[i] = new char[len + 1];
        m_names[i][len] = '\0';
        Memory::MemCpy(m_names[i], other.m_names[i], len);
    }
}

VMTypeInfo &VMTypeInfo::operator=(const VMTypeInfo &other)
{
    if (std::addressof(other) == this) {
        return *this;
    }

    delete[] m_name;

    for (SizeType i = 0; i < m_size; i++) {
        delete[] m_names[i];
    }

    if (m_size != other.m_size) {
        delete[] m_names;
        m_names = new char*[other.m_size];
    }

    m_size = other.m_size;

    SizeType name_len = std::strlen(other.m_name);
    m_name = new char[name_len + 1];
    m_name[name_len] = '\0';
    Memory::MemCpy(m_name, other.m_name, name_len);

    // copy all names
    for (SizeType i = 0; i < m_size; i++) {
        SizeType len = std::strlen(other.m_names[i]);
        m_names[i] = new char[len + 1];
        m_names[i][len] = '\0';
        Memory::MemCpy(m_names[i], other.m_names[i], len);
    }

    return *this;
}

VMTypeInfo::~VMTypeInfo()
{
    delete[] m_name;
    
    for (SizeType i = 0; i < m_size; i++) {
        delete[] m_names[i];
    }

    delete[] m_names;
}

bool VMTypeInfo::operator==(const VMTypeInfo &other) const
{
    if (std::addressof(other) == this) {
        return true;
    }

    // first, compare sizes
    if (m_size != other.m_size) {
        return false;
    }

    // compare type name
    if (std::strcmp(m_name, other.m_name)) {
        return false;
    }

    // compare names
    for (SizeType i = 0; i < m_size; i++) {
        if (std::strcmp(m_names[i], other.m_names[i])) {
            return false;
        }
    }

    return true;
}

} // namespace vm
} // namespace hyperion
