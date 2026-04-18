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

class Class;

class AssetBucket
{
public:
    static constexpr uint32 InvalidIndex = 0;

    constexpr explicit AssetBucket(uint32 index)
        : m_index(index)
    {
    }

    AssetBucket(const AssetBucket& other) = delete;
    AssetBucket& operator=(const AssetBucket& other) = delete;

    AssetBucket(AssetBucket&& other) noexcept = delete;
    AssetBucket& operator=(AssetBucket&& other) noexcept = delete;

    ~AssetBucket() = default;

    HYP_FORCE_INLINE constexpr explicit operator bool() const
    {
        return m_index != 0;
    }

    HYP_FORCE_INLINE constexpr bool operator!() const
    {
        return m_index == 0;
    }

    HYP_FORCE_INLINE constexpr bool operator==(const AssetBucket& other) const
    {
        return m_index == other.m_index;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const AssetBucket& other) const
    {
        return m_index != other.m_index;
    }

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
    inline constexpr AssetBucket None(0);

    inline constexpr AssetBucket Meshes(1);
    inline constexpr AssetBucket MaterialDefinitions(2);
    inline constexpr AssetBucket MaterialInstances(3);
    inline constexpr AssetBucket Textures(4);
    inline constexpr AssetBucket Lights(5);
    inline constexpr AssetBucket InstancedMeshData(6);
    inline constexpr AssetBucket Animations(7);
    inline constexpr AssetBucket AnimationTracks(8);
    inline constexpr AssetBucket Skeletons(9);
    inline constexpr AssetBucket Worlds(10);
    inline constexpr AssetBucket Scenes(11);
    inline constexpr AssetBucket Nodes(12);
    inline constexpr AssetBucket Entities(13);
    inline constexpr AssetBucket Bones(14);
    inline constexpr AssetBucket EnvProbes(15);
    inline constexpr AssetBucket LightmapVolumes(16);
} // namespace AssetBuckets

// adjust as needed
static constexpr size_t MaxAssetBuckets = 17;

/*! \brief Returns the predefined AssetBucket whose name matches \p nameHash, or nullptr. */
inline constexpr const AssetBucket& GetAssetBucketByName(StringHash nameHash)
{
    constexpr HashCode::ValueType MeshesHash = ("Meshes"_sh).hashCode;
    constexpr HashCode::ValueType MaterialDefinitionsHash = ("MaterialDefinitions"_sh).hashCode;
    constexpr HashCode::ValueType MaterialInstancesHash = ("MaterialInstances"_sh).hashCode;
    constexpr HashCode::ValueType TexturesHash = ("Textures"_sh).hashCode;
    constexpr HashCode::ValueType LightsHash = ("Lights"_sh).hashCode;
    constexpr HashCode::ValueType InstancedMeshDataHash = ("InstancedMeshData"_sh).hashCode;
    constexpr HashCode::ValueType AnimationsHash = ("Animations"_sh).hashCode;
    constexpr HashCode::ValueType AnimationTracksHash = ("AnimationTracks"_sh).hashCode;
    constexpr HashCode::ValueType SkeletonsHash = ("Skeletons"_sh).hashCode;
    constexpr HashCode::ValueType WorldsHash = ("Worlds"_sh).hashCode;
    constexpr HashCode::ValueType ScenesHash = ("Scenes"_sh).hashCode;
    constexpr HashCode::ValueType NodesHash = ("Nodes"_sh).hashCode;
    constexpr HashCode::ValueType EntitiesHash = ("Entities"_sh).hashCode;
    constexpr HashCode::ValueType BonesHash = ("Bones"_sh).hashCode;
    constexpr HashCode::ValueType EnvProbesHash = ("EnvProbes"_sh).hashCode;
    constexpr HashCode::ValueType LightmapVolumesHash = ("LightmapVolumes"_sh).hashCode;

    switch (nameHash.hashCode)
    {
    case MeshesHash:
        return AssetBuckets::Meshes;
    case MaterialDefinitionsHash:
        return AssetBuckets::MaterialDefinitions;
    case MaterialInstancesHash:
        return AssetBuckets::MaterialInstances;
    case TexturesHash:
        return AssetBuckets::Textures;
    case LightsHash:
        return AssetBuckets::Lights;
    case InstancedMeshDataHash:
        return AssetBuckets::InstancedMeshData;
    case AnimationsHash:
        return AssetBuckets::Animations;
    case AnimationTracksHash:
        return AssetBuckets::AnimationTracks;
    case SkeletonsHash:
        return AssetBuckets::Skeletons;
    case WorldsHash:
        return AssetBuckets::Worlds;
    case ScenesHash:
        return AssetBuckets::Scenes;
    case NodesHash:
        return AssetBuckets::Nodes;
    case EntitiesHash:
        return AssetBuckets::Entities;
    case BonesHash:
        return AssetBuckets::Bones;
    case EnvProbesHash:
        return AssetBuckets::EnvProbes;
    case LightmapVolumesHash:
        return AssetBuckets::LightmapVolumes;
    default:
        return AssetBuckets::None;
    }
}

} // namespace Hyperion
