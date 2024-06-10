#ifndef HYPERION_UUID_HPP
#define HYPERION_UUID_HPP

#include <core/containers/String.hpp>
#include <core/utilities/FormatFwd.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {
namespace utilities {

enum class UUIDVersion
{
    UUIDv3  = 3,
    UUIDv4  = 4
};

struct alignas(16) UUID
{
    uint64  data[2];

    UUID(UUIDVersion version = UUIDVersion::UUIDv4);

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const UUID &other) const
        { return data[0] == other.data[0] && data[1] == other.data[1]; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const UUID &other) const
        { return data[0] != other.data[0] || data[1] != other.data[1]; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(const UUID &other) const
        { return data[0] < other.data[0] || (data[0] == other.data[0] && data[1] < other.data[1]); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator>(const UUID &other) const
        { return data[0] > other.data[0] || (data[0] == other.data[0] && data[1] > other.data[1]); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<=(const UUID &other) const
        { return data[0] < other.data[0] || (data[0] == other.data[0] && data[1] <= other.data[1]); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator>=(const UUID &other) const
        { return data[0] > other.data[0] || (data[0] == other.data[0] && data[1] >= other.data[1]); }

    ANSIString ToString() const;

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(data[0]);
        hc.Add(data[1]);
        return hc;
    }

private:
    UUID(uint64 data0, uint64 data1)
        : data { data0, data1 }
    {}
};

} // namespace utilities

using utilities::UUID;

// formatter
namespace utilities {

template <class StringType>
struct Formatter<StringType, UUID>
{
    constexpr auto operator()(const UUID &value) const
    {
        return StringType(value.ToString());
    }
};

} // namespace utilities

} // namespace hyperion

#endif