/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UNIQUE_ID_HPP
#define HYPERION_UNIQUE_ID_HPP

#include <core/Defines.hpp>

#include <core/utilities/UUID.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {

namespace utilities {

struct UniqueID
{
public:
    UniqueID()
        : value(Generate().value)
    {
    }

    template <class T, typename = std::enable_if_t< !std::is_same_v< NormalizedType< T >, UniqueID > > >
    UniqueID(const T &value)
        : value(HashCode::GetHashCode(value).Value())
    {
    }

    UniqueID(const UniqueID &other)                 = default;
    UniqueID &operator=(const UniqueID &other)      = default;
    UniqueID(UniqueID &&other) noexcept             = default;
    UniqueID &operator=(UniqueID &&other) noexcept  = default;

    bool operator==(const UniqueID &other) const
        { return value == other.value; }

    bool operator!=(const UniqueID &other) const
        { return value != other.value; }

    bool operator<(const UniqueID &other) const
        { return value < other.value; }

    operator uint64() const
        { return value; }

    HashCode GetHashCode() const
        { return HashCode(value); }

    static inline UniqueID Generate()
    {
        return { UUID { } };
    }

private:
    uint64 value;
};

} // namespace utilities

using utilities::UniqueID;

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::UniqueID);

#endif