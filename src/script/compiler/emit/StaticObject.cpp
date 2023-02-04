#include <script/compiler/emit/StaticObject.hpp>
#include <system/Debug.hpp>

#include <Types.hpp>
#include <core/lib/CMemory.hpp>

namespace hyperion::compiler {

StaticObject::StaticObject()
    : m_id(0),
      m_type(TYPE_NONE)
{
}

StaticObject::StaticObject(int i)
    : m_id(0),
      m_type(TYPE_LABEL)
{
    m_value.lbl = i;
}

StaticObject::StaticObject(const char *str)
    : m_id(0),
      m_type(TYPE_STRING)
{
    m_value.str = str;
}

StaticObject::StaticObject(const StaticFunction &func)
    : m_id(0),
      m_type(TYPE_FUNCTION)
{
    m_value.func = func;
}

StaticObject::StaticObject(const StaticTypeInfo &type_info)
    : m_id(0),
      m_type(TYPE_TYPE_INFO)
{
    AssertThrowMsg(
        type_info.m_names.Size() == type_info.m_size,
        "number of names must be equal to the number of members"
    );
    
    m_value.type_info.m_name = type_info.m_name;
    m_value.type_info.m_size = type_info.m_size;
    m_value.type_info.m_names = type_info.m_names;
}

StaticObject::StaticObject(const StaticObject &other)
    : m_id(other.m_id),
      m_type(other.m_type)
{
    if (other.m_type == TYPE_LABEL) {
        m_value.lbl = other.m_value.lbl;
    } else if (other.m_type == TYPE_STRING) {
        m_value.str = other.m_value.str;
    } else if (other.m_type == TYPE_FUNCTION) {
        m_value.func = other.m_value.func;
    } else if (other.m_type == TYPE_TYPE_INFO) {
        m_value.type_info.m_name = other.m_value.type_info.m_name;
        m_value.type_info.m_names = other.m_value.type_info.m_names;
        m_value.type_info.m_size = other.m_value.type_info.m_size;
    }
}

StaticObject::~StaticObject() = default;

StaticObject &StaticObject::operator=(const StaticObject &other)
{
    m_id = other.m_id;
    m_type = other.m_type;
    
    if (other.m_type == TYPE_LABEL) {
        m_value.lbl = other.m_value.lbl;
    } else if (other.m_type == TYPE_STRING) {
        m_value.str = other.m_value.str;
    } else if (other.m_type == TYPE_FUNCTION) {
        m_value.func = other.m_value.func;
    } else if (other.m_type == TYPE_TYPE_INFO) {
        m_value.type_info.m_name = other.m_value.type_info.m_name;
        m_value.type_info.m_names = other.m_value.type_info.m_names;
        m_value.type_info.m_size = other.m_value.type_info.m_size;
    }

    return *this;
}

bool StaticObject::operator==(const StaticObject &other) const
{
    // do not compare id, we are checking for equality of the values
    if (m_type != other.m_type) {
        return false;
    }

    switch (m_type) {
    case TYPE_LABEL:
        return m_value.lbl == other.m_value.lbl;
    case TYPE_STRING:
        return m_value.str == other.m_value.str;
    case TYPE_FUNCTION:
        return m_value.func.m_addr == other.m_value.func.m_addr;
    case TYPE_TYPE_INFO:
        if (m_value.type_info.m_size != other.m_value.type_info.m_size) {
            return false;
        }

        if (m_value.type_info.m_name != other.m_value.type_info.m_name) {
            return false;
        }

        for (size_t i = 0; i < m_value.type_info.m_size; i++) {
            const NamesPair_t &a = m_value.type_info.m_names[i];
            const NamesPair_t &b = other.m_value.type_info.m_names[i];

            if (a.first != b.first) {
                return false;
            }
            
            if (std::memcmp(a.second.Data(), b.second.Data(), a.first) != 0) {
                return false;
            }
        }

        return true;
    default:
        return false;
    }
}

} // namespace hyperion::compiler
