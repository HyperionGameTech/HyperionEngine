#pragma once

#include <Lang/vm/Value.hpp>

#include <Core/containers/Map.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/reflection/BoxedValue.hpp>

#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <Core/debug/Debug.hpp>

namespace Hyperion {

class ScriptMap
{
public:
    struct VMMapKey
    {
        BoxedValue key;
        uint64 hash;

        HYP_FORCE_INLINE bool operator==(const VMMapKey& other) const
        {
            return hash == other.hash; // && key == other.key;
        }

        HYP_FORCE_INLINE bool operator!=(const VMMapKey& other) const
        {
            return hash != other.hash; // || key != other.key;
        }

        HYP_FORCE_INLINE HashCode GetHashCode() const
        {
            return HashCode().Add(hash);
        }
    };

    using InternalMapType = TMap<VMMapKey, BoxedValue, DynamicAllocator, HashTablePolicy::NotPooled>;

    ScriptMap();
    ScriptMap(const ScriptMap& other) = delete;
    ScriptMap& operator=(const ScriptMap& other) = delete;
    ScriptMap(ScriptMap&& other) noexcept;
    ScriptMap& operator=(ScriptMap&& other) noexcept;
    ~ScriptMap();

    size_t GetSize() const
    {
        return m_map.Size();
    }

    InternalMapType& GetMap()
    {
        return m_map;
    }

    const InternalMapType& GetMap() const
    {
        return m_map;
    }

    bool operator==(const ScriptMap& other) const
    {
        return this == &other;
    }

    void SetElement(VMMapKey&& key, BoxedValue&& value);

    BoxedValue* GetElement(const VMMapKey& key);
    const BoxedValue* GetElement(const VMMapKey& key) const;

private:
    InternalMapType m_map;
};

} // namespace Hyperion
