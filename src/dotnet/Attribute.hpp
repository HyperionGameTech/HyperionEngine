#pragma once
#include <core/memory/UniquePtr.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>

#include <core/Defines.hpp>

namespace hyperion::dotnet {

class ManagedObject;
class ManagedClass;

struct Attribute
{
    UniquePtr<ManagedObject> object;
};

class HYP_API AttributeSet
{
public:
    AttributeSet() = default;

    AttributeSet(Array<Attribute>&& values);

    AttributeSet(const AttributeSet& other) = delete;
    AttributeSet& operator=(const AttributeSet& other) = delete;

    AttributeSet(AttributeSet&& other) noexcept
        : m_values(std::move(other.m_values)),
          m_valuesByName(std::move(other.m_valuesByName))
    {
    }

    AttributeSet& operator=(AttributeSet&& other) noexcept
    {
        if (this != &other)
        {
            m_values = std::move(other.m_values);
            m_valuesByName = std::move(other.m_valuesByName);
        }

        return *this;
    }

    ~AttributeSet() = default;

    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_values.Size();
    }

    HYP_FORCE_INLINE bool HasAttribute(UTF8StringView name) const
    {
        return GetAttribute(name) != nullptr;
    }

    HYP_FORCE_INLINE Attribute* GetAttribute(UTF8StringView name) const
    {
        const auto it = m_valuesByName.FindAs(name);

        if (it == m_valuesByName.End())
        {
            return nullptr;
        }

        return it->second;
    }

    HYP_FORCE_INLINE Attribute* GetAttributeByHash(HashCode hashCode) const
    {
        const auto it = m_valuesByName.FindByHashCode(hashCode);

        if (it == m_valuesByName.End())
        {
            return nullptr;
        }

        return it->second;
    }

private:
    Array<Attribute> m_values;
    HashMap<String, Attribute*> m_valuesByName;
};

} // namespace hyperion::dotnet
