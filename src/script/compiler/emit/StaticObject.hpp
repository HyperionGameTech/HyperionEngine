#ifndef STATIC_OBJECT_HPP
#define STATIC_OBJECT_HPP

#include <script/compiler/emit/NamesPair.hpp>

#include <core/lib/String.hpp>

#include <Types.hpp>

#include <string>
#include <vector>

namespace hyperion::compiler {

struct StaticFunction
{
    UInt32 m_addr;
    UInt8 m_nargs;
    UInt8 m_flags;
};

struct StaticTypeInfo
{
    UInt8 m_size;
    ANSIString m_name;
    Array<NamesPair_t> m_names;
};

struct StaticObject
{
    int m_id;

    /*union*/ struct {
        int lbl;
        ANSIString str;
        StaticFunction func;
        StaticTypeInfo type_info;
    } m_value;

    enum {
        TYPE_NONE = 0,
        TYPE_LABEL,
        TYPE_STRING,
        TYPE_FUNCTION,
        TYPE_TYPE_INFO
    } m_type;

    StaticObject();
    explicit StaticObject(int i);
    explicit StaticObject(const char *str);
    explicit StaticObject(const StaticFunction &func);
    explicit StaticObject(const StaticTypeInfo &type_info);
    StaticObject(const StaticObject &other);
    ~StaticObject();

    StaticObject &operator=(const StaticObject &other);
    bool operator==(const StaticObject &other) const;
};

} // namespace hyperion::compiler

#endif
