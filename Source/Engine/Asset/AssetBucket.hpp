/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Name/Name.hpp>

namespace Hyperion {

class Class;

#define HYP_FOR_EACH_ASSET_BUCKET(X) \
    X(Meshes,               1)       \
    X(Materials,            2)       \
    X(Textures,             3)       \
    X(Lights,               4)       \
    X(InstancedMeshData,    5)       \
    X(Animations,           6)       \
    X(AnimationTracks,      7)       \
    X(Skeletons,            8)       \
    X(Worlds,               9)       \
    X(Scenes,               10)      \
    X(Nodes,                11)      \
    X(Entities,             12)      \
    X(Bones,                13)      \
    X(EnvProbes,            14)      \
    X(LightmapVolumes,      15)      \
    X(Shaders,              16)      \
    X(ShaderBundles,        17)      \
    X(FontAtlases,          18)      \
    X(PhysicsShapes,        19)      \
    X(Scripts,              20)      \
    X(Sprites,              21)

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

#define HYP_ASSET_BUCKET_DEF(Name, Index) \
    inline constexpr AssetBucket Name(Index);
    HYP_FOR_EACH_ASSET_BUCKET(HYP_ASSET_BUCKET_DEF)
#undef HYP_ASSET_BUCKET_DEF

    static constexpr const AssetBucket* AllBuckets[] = {
        &None,
#define HYP_ASSET_BUCKET_PTR(Name, Index) &Name,
        HYP_FOR_EACH_ASSET_BUCKET(HYP_ASSET_BUCKET_PTR)
#undef HYP_ASSET_BUCKET_PTR
    };
} // namespace AssetBuckets

static constexpr size_t MaxAssetBuckets = std::size(AssetBuckets::AllBuckets);

/*! \brief Returns the predefined AssetBucket whose name matches \p nameHash, or nullptr. */
inline constexpr const AssetBucket& GetAssetBucketByName(StringHash nameHash)
{
#define HYP_ASSET_BUCKET_HASH(Name, Index) \
    constexpr HashCode::ValueType Name##Hash = StringHash(#Name).hashCode;
    HYP_FOR_EACH_ASSET_BUCKET(HYP_ASSET_BUCKET_HASH)
#undef HYP_ASSET_BUCKET_HASH

    switch (nameHash.hashCode)
    {
#define HYP_ASSET_BUCKET_CASE(Name, Index) \
    case Name##Hash: return AssetBuckets::Name;
        HYP_FOR_EACH_ASSET_BUCKET(HYP_ASSET_BUCKET_CASE)
#undef HYP_ASSET_BUCKET_CASE
    }

    return AssetBuckets::None;
}

inline const char* GetAssetBucketName(const uint32 bucketIndex)
{
    static constexpr const char* s_names[MaxAssetBuckets] = {
        nullptr, // 0 = None
#define HYP_ASSET_BUCKET_NAME(Name, Index) #Name,
        HYP_FOR_EACH_ASSET_BUCKET(HYP_ASSET_BUCKET_NAME)
#undef HYP_ASSET_BUCKET_NAME
    };

    if (bucketIndex == 0 || bucketIndex >= MaxAssetBuckets)
    {
        return nullptr;
    }

    return s_names[bucketIndex];
}

#undef HYP_FOR_EACH_ASSET_BUCKET

} // namespace Hyperion
