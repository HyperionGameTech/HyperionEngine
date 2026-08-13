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

// clang-format off

#define HYP_FOR_EACH_ASSET_BUCKET(X) \
    X(Meshes, 1)                     \
    X(Textures, 2)                   \
    X(Materials, 3)                  \
    X(InstancedMeshData, 4)          \
    X(Animations, 5)                 \
    X(AnimationTracks, 6)            \
    X(Skeletons, 7)                  \
    X(Worlds, 8)                     \
    X(Scenes, 9)                     \
    X(Shaders, 10)                   \
    X(ShaderBundles, 11)             \
    X(FontAtlases, 12)               \
    X(PhysicsShapes, 13)             \
    X(Scripts, 14)                   \
    X(RawData, 15)                   \
    X(Prefabs, 16)                   \
    X(Sounds, 17)

// clang-format on

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
static constexpr const char* AllBucketNameStrings[] = {
    nullptr, // None
#define HYP_ASSET_BUCKET_STR(Name, Index) HYP_STR(Name),
    HYP_FOR_EACH_ASSET_BUCKET(HYP_ASSET_BUCKET_STR)
#undef HYP_ASSET_BUCKET_STR
};

} // namespace AssetBuckets

static constexpr size_t MaxAssetBuckets = GetArrayCount(AssetBuckets::AllBuckets);

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
    case Name##Hash:                       \
        return AssetBuckets::Name;
        HYP_FOR_EACH_ASSET_BUCKET(HYP_ASSET_BUCKET_CASE)
#undef HYP_ASSET_BUCKET_CASE
    }

    return AssetBuckets::None;
}

inline const char* GetAssetBucketName(const uint32 bucketIndex)
{
    if (bucketIndex == 0 || bucketIndex >= MaxAssetBuckets)
    {
        return nullptr;
    }

    return AssetBuckets::AllBucketNameStrings[bucketIndex];
}

#undef HYP_FOR_EACH_ASSET_BUCKET

} // namespace Hyperion
