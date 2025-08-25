#pragma once

#include <core/containers/HashMap.hpp>
#include <script/vm/Value.hpp>
#include <core/math/MathUtil.hpp>
#include <core/Types.hpp>
#include <core/HashCode.hpp>

#include <core/debug/Debug.hpp>

#include <sstream>

namespace hyperion {
namespace vm {

class VMMap
{
public:
    struct VMMapKey
    {
        Value key;
        uint64 hash;

        HYP_FORCE_INLINE bool operator==(const VMMapKey& other) const
        {
            return hash == other.hash;// && key == other.key;
        }

        HYP_FORCE_INLINE bool operator!=(const VMMapKey& other) const
        {
            return hash != other.hash;// || key != other.key;
        }

        HYP_FORCE_INLINE HashCode GetHashCode() const
        {
            return HashCode().Add(hash);
        }
    };

    using InternalMapType = HashMap<VMMapKey, Value, HashTable_DynamicNodeAllocator<KeyValuePair<VMMapKey, Value>>>;

    VMMap();
    VMMap(const VMMap& other) = delete;
    VMMap& operator=(const VMMap& other) = delete;
    VMMap(VMMap&& other) noexcept;
    VMMap& operator=(VMMap&& other) noexcept;
    ~VMMap();

    SizeType GetSize() const
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

    bool operator==(const VMMap& other) const
    {
        return this == &other;
    }

    void SetElement(VMMapKey&& key, Value&& value);

    Value* GetElement(const VMMapKey& key);
    const Value* GetElement(const VMMapKey& key) const;

    void GetRepresentation(
        std::stringstream& ss,
        bool addTypeName = true,
        int depth = 3) const;

private:
    InternalMapType m_map;
};

} // namespace vm
} // namespace hyperion

