#pragma once

#include <Lang/VM/Value.hpp>
#include <Lang/VM/ScriptMemory.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <Core/Debug/Debug.hpp>

namespace Hyperion {

struct ScriptMapKey
{
    BoxedValue key;

    HYP_FORCE_INLINE bool operator==(const ScriptMapKey& other) const
    {
        if (GetHashCode() != other.GetHashCode())
        {
            return false;
        }

        // For inline types (Name, int64, etc.) the Variant comparison works directly.
        // For Any-wrapped types (String, GenericArrayWrapper), Map stores independent copies
        // via ShallowCopy, but Any::operator== is pointer-based, so two distinct copies
        // with identical content would compare as unequal. Fall through to value comparison.
        if (key.value == other.key.value)
        {
            return true;
        }

        return ScriptMapKey::ValuesEqual(key, other.key);
    }

    HYP_FORCE_INLINE bool operator!=(const ScriptMapKey& other) const
    {
        return !operator==(other);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return GetHashCodeStatic(key);
    }

    SCRIPT_API static HashCode GetHashCodeStatic(const BoxedValue& key);
    
    SCRIPT_API static bool ValuesEqual(const BoxedValue& a, const BoxedValue& b);
};

class ScriptMap final : public Map<ScriptMapKey, BoxedValue, ScriptAllocator, HashTablePolicy::NotPooled>
{
public:
    using Base = Map;

    ScriptMap() = default;

    ScriptMap(const ScriptMap& other) = default;
    ScriptMap& operator=(const ScriptMap& other) = default;

    ScriptMap(ScriptMap&& other) noexcept = default;
    ScriptMap& operator=(ScriptMap&& other) noexcept = default;

    ~ScriptMap() = default;

    size_t GetSize() const
    {
        return Base::Size();
    }

    Base& GetMap() &
    {
        return *static_cast<Base*>(this);
    }

    const Base& GetMap() const&
    {
        return *static_cast<const Base*>(this);
    }

    using Base::operator==;
    using Base::operator!=;
    using Base::GetHashCode;

    SCRIPT_API void SetElement(ScriptMapKey&& key, BoxedValue&& value);

    SCRIPT_API BoxedValue* GetElement(const ScriptMapKey& key);
    SCRIPT_API const BoxedValue* GetElement(const ScriptMapKey& key) const;
};

} // namespace Hyperion
