#pragma once

#include <Lang/compiler/emit/NamesPair.hpp>

#include <Core/containers/String.hpp>

#include <Core/Types.hpp>

#include <string>
#include <vector>

namespace Hyperion {

struct StaticFunction
{
    uint32 m_addr;
    uint8 m_nargs;
    uint8 m_flags;
};

struct StaticTypeInfo
{
    uint8 m_size;
    String m_name;
    Array<NamesPair_t> m_names;
};

struct StaticObject
{
    int m_id;

    /*union*/ struct
    {
        int lbl;
        String str;
        StaticFunction func;
        StaticTypeInfo typeInfo;
    } m_value;

    enum
    {
        TYPE_NONE = 0,
        TYPE_LABEL,
        TYPE_STRING,
        TYPE_FUNCTION,
        TYPE_TYPE_INFO
    } m_type;

    StaticObject();
    explicit StaticObject(int i);
    explicit StaticObject(const char* str);
    explicit StaticObject(const StaticFunction& func);
    explicit StaticObject(const StaticTypeInfo& typeInfo);
    StaticObject(const StaticObject& other);
    ~StaticObject();

    StaticObject& operator=(const StaticObject& other);
    bool operator==(const StaticObject& other) const;
};

} // namespace Hyperion
