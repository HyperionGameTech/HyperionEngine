#pragma once

#include <Lang/vm/Value.hpp>
#include <Lang/vm/ScriptMemory.hpp>

#include <Core/containers/Map.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/reflection/BoxedValue.hpp>

#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <Core/debug/Debug.hpp>

namespace Hyperion {

struct ScriptMapKey
{
    BoxedValue key;
    uint64 hash;

    HYP_FORCE_INLINE bool operator==(const ScriptMapKey& other) const
    {
        return hash == other.hash; // && key == other.key;
    }

    HYP_FORCE_INLINE bool operator!=(const ScriptMapKey& other) const
    {
        return hash != other.hash; // || key != other.key;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.value = hash;
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

    void SetElement(ScriptMapKey&& key, BoxedValue&& value);

    BoxedValue* GetElement(const ScriptMapKey& key);
    const BoxedValue* GetElement(const ScriptMapKey& key) const;
};

} // namespace Hyperion
