#pragma once

#include <script/vm/Value.hpp>

#include <core/containers/HashMap.hpp>

#include <core/math/MathUtil.hpp>

#include <core/reflection/HypData.hpp>

#include <core/Types.hpp>
#include <core/HashCode.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

class Script_HashMap
{
public:
    struct VMMapKey
    {
        HypData key;
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

    using InternalMapType = HashMap<VMMapKey, HypData, DynamicNodeAllocator>;

    Script_HashMap();
    Script_HashMap(const Script_HashMap& other) = delete;
    Script_HashMap& operator=(const Script_HashMap& other) = delete;
    Script_HashMap(Script_HashMap&& other) noexcept;
    Script_HashMap& operator=(Script_HashMap&& other) noexcept;
    ~Script_HashMap();

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

    bool operator==(const Script_HashMap& other) const
    {
        return this == &other;
    }

    void SetElement(VMMapKey&& key, HypData&& value);

    HypData* GetElement(const VMMapKey& key);
    const HypData* GetElement(const VMMapKey& key) const;

private:
    InternalMapType m_map;
};

} // namespace hyperion
