/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>

#include <core/utilities/FormatFwd.hpp>
#include <core/utilities/ByteUtil.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

namespace Hyperion {
namespace utilities {

HYP_STRUCT(Serialize = "bitwise")
struct HYP_API Uuid
{
    HYP_STRUCT_BODY(Uuid);

    HYP_FIELD(Serialize, Property = "Data0")
    uint64 data0;

    HYP_FIELD(Serialize, Property = "Data1")
    uint64 data1;

    constexpr Uuid(uint64 data0, uint64 data1)
        : data0 { data0 },
          data1 { data1 }
    {
    }

    Uuid();
    explicit Uuid(const char* str);

    HYP_FORCE_INLINE constexpr bool operator==(const Uuid& other) const
    {
        return data0 == other.data0 && data1 == other.data1;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const Uuid& other) const
    {
        return data0 != other.data0 || data1 != other.data1;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const Uuid& other) const
    {
        return data0 < other.data0 || (data0 == other.data0 && data1 < other.data1);
    }

    HYP_FORCE_INLINE constexpr bool operator>(const Uuid& other) const
    {
        return data0 > other.data0 || (data0 == other.data0 && data1 > other.data1);
    }

    HYP_FORCE_INLINE constexpr bool operator<=(const Uuid& other) const
    {
        return data0 < other.data0 || (data0 == other.data0 && data1 <= other.data1);
    }

    HYP_FORCE_INLINE constexpr bool operator>=(const Uuid& other) const
    {
        return data0 > other.data0 || (data0 == other.data0 && data1 >= other.data1);
    }

    HYP_METHOD()
    String ToString() const;

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(data0)
            .Combine(HashCode::GetHashCode(data1));
    }

    HYP_FORCE_INLINE constexpr static Uuid Invalid()
    {
        return { 0, 0 };
    }
};

} // namespace utilities

using utilities::Uuid;

// formatter
namespace utilities {

template <class StringType>
struct Formatter<StringType, Uuid>
{
    constexpr auto operator()(const Uuid& value) const
    {
        return StringType(value.ToString());
    }
};

constexpr Uuid SwapEndian(Uuid value)
{
    Uuid result = Uuid::Invalid();
    result.data0 = SwapEndian(value.data0);
    result.data1 = SwapEndian(value.data1);

    return result;
}

} // namespace utilities

} // namespace Hyperion
