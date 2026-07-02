#include <Lang/VM/Map.hpp>

#include <Core/Debug/Debug.hpp>

#include <cmath>
#include <cstring>
#include <sstream>

namespace Hyperion {

#pragma region ScriptMapKey

HashCode ScriptMapKey::GetHashCodeStatic(const BoxedValue& key)
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

bool ScriptMapKey::ValuesEqual(const BoxedValue& a, const BoxedValue& b)
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

    if (aTypeId.Value() == CONSTEXPR_TYPE_ID(Name))
    {
        return aRef.GetUnchecked<Name>() == bRef.GetUnchecked<Name>();
    }

    if (aTypeId.Value() == CONSTEXPR_TYPE_ID(String))
    {
        return aRef.GetUnchecked<String>() == bRef.GetUnchecked<String>();
    }

    return false;
}

#pragma endregion ScriptMapKey

#pragma region ScriptMap

void ScriptMap::SetElement(ScriptMapKey&& key, BoxedValue&& value)
{
    auto it = Base::Find(key);

    if (it == Base::End())
    {
        Base::Insert(std::move(key), std::move(value));
        return;
    }

    (void)key;
    it->second = std::move(value);
}

BoxedValue* ScriptMap::GetElement(const ScriptMapKey& key)
{
    auto it = Base::Find(key);

    if (it == Base::End())
    {
        return nullptr;
    }

    return &it->second;
}

const BoxedValue* ScriptMap::GetElement(const ScriptMapKey& key) const
{
    auto it = Base::Find(key);

    if (it == Base::End())
    {
        return nullptr;
    }

    return &it->second;
}

#pragma endregion ScriptMap

} // namespace Hyperion
