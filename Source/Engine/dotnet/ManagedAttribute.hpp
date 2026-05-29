#pragma once
#include <Core/memory/UniquePtr.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/Map.hpp>
#include <Core/containers/String.hpp>

#include <Core/Defines.hpp>

namespace Hyperion::dotnet {

class ManagedObject;
class ManagedClass;

class ENGINE_API ManagedAttributeSet
{
public:
    ManagedAttributeSet() = default;

    explicit ManagedAttributeSet(Array<UniquePtr<ManagedObject>>&& values);

    ManagedAttributeSet(const ManagedAttributeSet& other) = delete;
    ManagedAttributeSet& operator=(const ManagedAttributeSet& other) = delete;

    ManagedAttributeSet(ManagedAttributeSet&& other) noexcept
        : m_values(std::move(other.m_values)),
          m_valuesByName(std::move(other.m_valuesByName))
    {
    }

    ManagedAttributeSet& operator=(ManagedAttributeSet&& other) noexcept
    {
        if (this != &other)
        {
            m_values = std::move(other.m_values);
            m_valuesByName = std::move(other.m_valuesByName);
        }

        return *this;
    }

    ~ManagedAttributeSet() = default;

    HYP_FORCE_INLINE size_t Size() const
    {
        return m_values.Size();
    }

    HYP_FORCE_INLINE bool HasAttribute(UTF8StringView name) const
    {
        return GetAttribute(name) != nullptr;
    }

    HYP_FORCE_INLINE ManagedObject* GetAttribute(UTF8StringView name) const
    {
        const auto it = m_valuesByName.FindAs(name);

        if (it == m_valuesByName.End())
        {
            return nullptr;
        }

        return it->second;
    }

    HYP_FORCE_INLINE ManagedObject* GetAttributeByHash(HashCode hashCode) const
    {
        const auto it = m_valuesByName.FindByHashCode(hashCode);

        if (it == m_valuesByName.End())
        {
            return nullptr;
        }

        return it->second;
    }

private:
    Array<UniquePtr<ManagedObject>> m_values;
    TMap<String, ManagedObject*> m_valuesByName;
};

} // namespace Hyperion::dotnet
