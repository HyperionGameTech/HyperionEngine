#pragma once

#include <core/debug/Debug.hpp>

#include <core/Types.hpp>

namespace hyperion {

class VMTypeInfo
{
public:
    VMTypeInfo(const char* name, SizeType size, char** names);
    VMTypeInfo(const VMTypeInfo& other);
    VMTypeInfo& operator=(const VMTypeInfo& other);
    ~VMTypeInfo();

    bool operator==(const VMTypeInfo& other) const;

    char* const GetName() const
    {
        return m_name;
    }

    SizeType GetSize() const
    {
        return m_size;
    }

    char** const GetNames() const
    {
        return m_names;
    }

    const char* GetMemberName(SizeType index) const
    {
        return m_names[index];
    }

private:
    char* m_name;
    SizeType m_size;
    char** m_names;
};

} // namespace hyperion
