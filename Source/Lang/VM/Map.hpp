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
        // For Any-wrapped types (String, GenericArrayWrapper), TMap stores independent copies
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

    HYP_FORCE_INLINE static HashCode GetHashCodeStatic(const BoxedValue& key)
    {
        HashCode hc;

        key.value.Visit([&hc, &key](auto&& value)
            {
                using T = NormalizedType<decltype(value)>;

                if constexpr (std::is_same_v<T, AnyRef>)
                {
                    const AnyRef& ref = value;

                    if (ref.Is<Name>())
                    {
                        hc = ref.GetUnchecked<Name>().GetHashCode();
                    }
                    else
                    {
                        hc = HashCode::GetHashCode(ref.GetPointer());
                    }
                }
                else if constexpr (std::is_same_v<T, Any>)
                {
                    // For Any-wrapped values, resolve the underlying type through ToRef
                    const AnyRef& ref = key.ToRef();

                    if (ref.Is<String>())
                    {
                        hc = ref.GetUnchecked<String>().GetHashCode();
                    }
                    else if (ref.Is<Name>())
                    {
                        hc = ref.GetUnchecked<Name>().GetHashCode();
                    }
                    else
                    {
                        hc = HashCode::GetHashCode(ref.GetPointer());
                    }
                }
                else if constexpr (std::is_fundamental_v<T>)
                {
                    hc = HashCode::GetHashCode(value);
                }
                else if constexpr (HYP_HAS_METHOD(T, GetHashCode))
                {
                    hc = value.GetHashCode();
                }
                else
                {
                    hc = HashCode::GetHashCode(key.ToRef().GetPointer());
                }
            });

        return hc;
    }

    HYP_FORCE_INLINE static bool ValuesEqual(const BoxedValue& a, const BoxedValue& b)
    {
        const AnyRef aRef = a.ToRef();
        const AnyRef bRef = b.ToRef();

        if (!aRef.HasValue() || !bRef.HasValue())
        {
            return aRef.HasValue() == bRef.HasValue();
        }

        const TypeId aTypeId = aRef.GetTypeId();
        const TypeId bTypeId = bRef.GetTypeId();

        if (aTypeId != bTypeId)
        {
            return false;
        }

        if (aTypeId == TypeId::ForType<Name>())
        {
            return aRef.GetUnchecked<Name>() == bRef.GetUnchecked<Name>();
        }

        if (aTypeId == TypeId::ForType<String>())
        {
            return aRef.GetUnchecked<String>() == bRef.GetUnchecked<String>();
        }

        return false;
    }

};

class ScriptMap final : public TMap<ScriptMapKey, BoxedValue, ScriptAllocator, HashTablePolicy::NotPooled>
{
public:
    using Base = TMap;

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
