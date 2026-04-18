/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/name/Name.hpp>

namespace Hyperion {

class AssetBucket
{
public:
    constexpr explicit AssetBucket(uint32 index)
        : m_index(index)
    {
    }

    AssetBucket(const AssetBucket& other) = delete;
    AssetBucket& operator=(const AssetBucket& other) = delete;

    AssetBucket(AssetBucket&& other) noexcept = delete;
    AssetBucket& operator=(AssetBucket&& other) noexcept = delete;

    ~AssetBucket() = default;

    HYP_FORCE_INLINE constexpr uint32 GetIndex() const
    {
        return m_index;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode(HashCode::ValueType(m_index));
    }

private:
    uint32 m_index;
};

namespace AssetBuckets {
    inline constexpr AssetBucket Meshes(0);
    inline constexpr AssetBucket MaterialDefinitions(1);
    inline constexpr AssetBucket MaterialInstances(2);
    inline constexpr AssetBucket Textures(3);
    inline constexpr AssetBucket Lights(4);
    inline constexpr AssetBucket InstancedMeshData(5);
    inline constexpr AssetBucket Animations(6);
    inline constexpr AssetBucket AnimationTracks(7);
    inline constexpr AssetBucket Skeletons(8);
    inline constexpr AssetBucket Worlds(9);
    inline constexpr AssetBucket Scenes(10);
    inline constexpr AssetBucket Nodes(11);
    inline constexpr AssetBucket Entities(12);
    inline constexpr AssetBucket Bones(13);
    inline constexpr AssetBucket EnvProbes(14);
} // namespace AssetBuckets

} // namespace Hyperion
