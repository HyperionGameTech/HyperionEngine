/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <asset/AssetBucket.hpp>

#include <Core/reflection/ObjectFwd.hpp>

#include <Core/name/Name.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/String.hpp>

#include <Core/utilities/FormatFwd.hpp>

namespace Hyperion {

#pragma pack(push, 1)

HYP_STRUCT()
struct AssetPath
{
    HYP_STRUCT_BODY(AssetPath);

    HYP_PROPERTY(Value, &AssetPath::ToString, &AssetPath::Set)

    HYP_FIELD(NoScriptBindings, Transient)
    Name assetName;

    HYP_FIELD(NoScriptBindings, Transient)
    uint32 bucketIndex = AssetBucket::InvalidIndex;

    constexpr AssetPath() = default;

    explicit AssetPath(const UTF8StringView& path);

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
        *this = AssetPath(path);
    }

    HYP_METHOD()
    String ToString() const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return ToString().GetHashCode();
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
