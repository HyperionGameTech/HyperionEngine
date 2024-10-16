#ifndef HYPERION_DOTNET_ATTRIBUTE_HPP
#define HYPERION_DOTNET_ATTRIBUTE_HPP

#include <core/memory/UniquePtr.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/StringView.hpp>

#include <core/Defines.hpp>

namespace hyperion::dotnet {

class Object;
class Class;

struct Attribute
{
    UniquePtr<Object>   object;
};

class HYP_API AttributeSet
{
public:
    AttributeSet()                                          = default;

    AttributeSet(Array<Attribute> &&values);

    AttributeSet(const AttributeSet &other)                 = delete;
    AttributeSet &operator=(const AttributeSet &other)      = delete;
    AttributeSet(AttributeSet &&other) noexcept             = default;
    AttributeSet &operator=(AttributeSet &&other) noexcept  = default;
    ~AttributeSet()                                         = default;

    HYP_FORCE_INLINE bool HasAttribute(UTF8StringView name) const
        { return m_values_by_name.Contains(name); }

    HYP_FORCE_INLINE Attribute *GetAttribute(UTF8StringView name) const
    {
        const auto it = m_values_by_name.FindAs(name);

        if (it == m_values_by_name.End()) {
            return nullptr;
        }

        return it->second;
    }

private:
    Array<Attribute>                m_values;
    HashMap<String, Attribute *>    m_values_by_name;
};

} // namespace hyperion::dotnet

#endif