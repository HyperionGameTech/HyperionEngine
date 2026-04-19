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

const char* GetAssetBucketName(const uint32 bucketIndex);

HYP_STRUCT()
class AssetBucket
{
public:
    HYP_STRUCT_BODY(AssetBucket);

public:

    static constexpr uint32 InvalidIndex = 0;
    
    constexpr AssetBucket()
        : m_index(InvalidIndex)
    {
    }

    constexpr explicit AssetBucket(uint32 index)
        : m_index(index)
    {
    }

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

    HYP_FORCE_INLINE const char* GetName() const
    {
        return GetAssetBucketName(m_index);
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
    inline constexpr AssetBucket Shaders(17);
    inline constexpr AssetBucket ShaderBundles(18);
    inline constexpr AssetBucket FontAtlases(19);

    static constexpr const AssetBucket* AllBuckets[] = {
        &None,
        &Meshes,
        &MaterialDefinitions,
        &MaterialInstances,
        &Textures,
        &Lights,
        &InstancedMeshData,
        &Animations,
        &AnimationTracks,
        &Skeletons,
        &Worlds,
        &Scenes,
        &Nodes,
        &Entities,
        &Bones,
        &EnvProbes,
        &LightmapVolumes,
        &Shaders,
        &ShaderBundles,
        &FontAtlases
    };
} // namespace AssetBuckets

// adjust as needed
static constexpr size_t MaxAssetBuckets = std::size(AssetBuckets::AllBuckets);

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
    constexpr HashCode::ValueType ShadersHash = ("Shaders"_sh).hashCode;
    constexpr HashCode::ValueType ShaderBundlesHash = ("ShaderBundles"_sh).hashCode;
    constexpr HashCode::ValueType FontAtlasesHash = ("FontAtlases"_sh).hashCode;

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
    case ShadersHash:
        return AssetBuckets::Shaders;
    case ShaderBundlesHash:
        return AssetBuckets::ShaderBundles;
    case FontAtlasesHash:
        return AssetBuckets::FontAtlases;
    }

    return AssetBuckets::None;
}

inline static const char* GetAssetBucketName(const uint32 bucketIndex)
{
    static constexpr const char* s_names[MaxAssetBuckets] = {
        nullptr,                // 0 = None
        "Meshes",               // 1
        "MaterialDefinitions",  // 2
        "MaterialInstances",    // 3
        "Textures",             // 4
        "Lights",               // 5
        "InstancedMeshData",    // 6
        "Animations",           // 7
        "AnimationTracks",      // 8
        "Skeletons",            // 9
        "Worlds",               // 10
        "Scenes",               // 11
        "Nodes",                // 12
        "Entities",             // 13
        "Bones",                // 14
        "EnvProbes",            // 15
        "LightmapVolumes",      // 16
        "Shaders",              // 17
        "ShaderBundles",        // 18
        "FontAtlases"           // 19
    };

    if (bucketIndex == 0 || bucketIndex >= MaxAssetBuckets)
    {
        return nullptr;
    }

    return s_names[bucketIndex];
}

} // namespace Hyperion
