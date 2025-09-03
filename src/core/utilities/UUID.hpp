/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>

#include <core/utilities/FormatFwd.hpp>
#include <core/utilities/ByteUtil.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

namespace hyperion {
namespace utilities {

enum class UUIDVersion
{
    UUIDv3 = 3,
    UUIDv4 = 4
};

HYP_STRUCT(Serialize = "bitwise")
struct UUID
{
    HYP_FIELD(Serialize, Property = "Data0")
    uint64 data0;

    HYP_FIELD(Serialize, Property = "Data1")
    uint64 data1;

    constexpr UUID(uint64 data0, uint64 data1)
        : data0 { data0 },
          data1 { data1 }
    {
    }

    HYP_API explicit UUID(const ANSIStringView& str);

    UUID(UUIDVersion version = UUIDVersion::UUIDv4);

    HYP_FORCE_INLINE constexpr bool operator==(const UUID& other) const
    {
        return data0 == other.data0 && data1 == other.data1;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const UUID& other) const
    {
        return data0 != other.data0 || data1 != other.data1;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const UUID& other) const
    {
        return data0 < other.data0 || (data0 == other.data0 && data1 < other.data1);
    }

    HYP_FORCE_INLINE constexpr bool operator>(const UUID& other) const
    {
        return data0 > other.data0 || (data0 == other.data0 && data1 > other.data1);
    }

    HYP_FORCE_INLINE constexpr bool operator<=(const UUID& other) const
    {
        return data0 < other.data0 || (data0 == other.data0 && data1 <= other.data1);
    }

    HYP_FORCE_INLINE constexpr bool operator>=(const UUID& other) const
    {
        return data0 > other.data0 || (data0 == other.data0 && data1 >= other.data1);
    }

    HYP_METHOD()
    HYP_API ANSIString ToString() const;

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(data0)
            .Combine(HashCode::GetHashCode(data1));
    }

    HYP_FORCE_INLINE constexpr static UUID Invalid()
    {
        return { 0, 0 };
    }
};

} // namespace utilities

using utilities::UUID;

// formatter
namespace utilities {

template <class StringType>
struct Formatter<StringType, UUID>
{
    constexpr auto operator()(const UUID& value) const
    {
        return StringType(value.ToString());
    }
};

constexpr UUID SwapEndian(UUID value)
{
    UUID result = UUID::Invalid();
    result.data0 = SwapEndian(value.data0);
    result.data1 = SwapEndian(value.data1);

    return result;
}

} // namespace utilities

} // namespace hyperion
