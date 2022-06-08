#ifndef STATIC_OBJECT_HPP
#define STATIC_OBJECT_HPP

#include <script/compiler/emit/NamesPair.hpp>

#include <string>
#include <vector>

namespace hyperion {
namespace compiler {

struct StaticFunction {
    uint32_t m_addr;
    uint8_t m_nargs;
    uint8_t m_flags;
};

struct StaticTypeInfo {
    uint8_t m_size;
    char *m_name;
    std::vector<NamesPair_t> m_names;
};

struct StaticObject {
    int m_id;

    /*union*/ struct {
        int lbl;
        char *str;
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

} // namespace compiler
} // namespace hyperion

#endif
