/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetBucket.hpp>

#include <Core/reflection/ObjectFwd.hpp>

#include <Core/name/Name.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/String.hpp>

#include <Core/utilities/FormatFwd.hpp>

namespace Hyperion {

#pragma pack(push, 1)

HYP_ENUM()
enum class AssetRegistryId : uint32
{
    Game = 0,
    Engine,
    Editor
};

HYP_STRUCT()
struct ENGINE_API AssetPath
{
    HYP_STRUCT_BODY(AssetPath);

    HYP_PROPERTY(Value, &AssetPath::ToString, &AssetPath::Set)

    Name assetName;
    AssetRegistryId registryId : 3;
    uint32 bucketIndex : 29;

    constexpr AssetPath()
        : registryId(AssetRegistryId::Game),
          bucketIndex(AssetBucket::InvalidIndex)
    {
    }

    explicit AssetPath(const ANSIStringView& path);

    constexpr AssetPath(AssetRegistryId registryId, const AssetBucket& bucket, Name assetName)
        : assetName(assetName),
          registryId(registryId),
          bucketIndex(bucket.GetIndex())
    {
    }

    constexpr AssetPath(const AssetBucket& bucket, Name assetName)
        : assetName(assetName),
          registryId(AssetRegistryId::Game),
          bucketIndex(bucket.GetIndex())
    {
    }

    constexpr AssetPath(const AssetPath& other) = default;
    AssetPath& operator=(const AssetPath& other) = default;

    constexpr AssetPath(AssetPath&& other) noexcept = default;
    AssetPath& operator=(AssetPath&& other) noexcept = default;

    constexpr bool operator==(const AssetPath& other) const = default;
    constexpr bool operator!=(const AssetPath& other) const = default;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsValid() const
    {
        return assetName.IsValid()
            && bucketIndex != AssetBucket::InvalidIndex;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const AssetBucket& GetBucket() const
    {
        return *AssetBuckets::AllBuckets[bucketIndex];
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !IsValid();
    }

    HYP_METHOD()
    Name GetName() const
    {
        return assetName;
    }

    void Set(const String& path)
    {
        *this = AssetPath(ANSIStringView(*path));
    }

    HYP_METHOD()
    String ToString() const;

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode(assetName.hashCode)
            .Combine(HashCode::ValueType((uint32(registryId) & 0x7) | (uint32(bucketIndex) << 3)));
    }
};

#pragma pack(pop)

// Formatter for AssetPath
namespace utilities {

template <class StringType>
struct Formatter<StringType, AssetPath>
{
    auto operator()(const AssetPath& value) const
    {
        return value.ToString();
    }
};

} // namespace utilities

} // namespace Hyperion
