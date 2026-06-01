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
        return GetHashCode() == other.GetHashCode() && key.value == other.key.value;
    }

    HYP_FORCE_INLINE bool operator!=(const ScriptMapKey& other) const
    {
        return GetHashCode() != other.GetHashCode() || key.value != other.key.value;
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
                else if constexpr (std::is_fundamental_v<T>)
                {
                    hc = HashCode::GetHashCode(value);
                }
                else if constexpr (HYP_HAS_METHOD(T, GetHashCode))
                {
                    hc = value.GetHashCode();
                }
                else if (key.Is<String>())
                {
                    hc = key.Get<String>().GetHashCode();
                }
                else
                {
                    hc = HashCode::GetHashCode(key.ToRef().GetPointer());
                }
            });

        return hc;
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
