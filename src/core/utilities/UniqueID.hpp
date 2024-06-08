/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UNIQUE_ID_HPP
#define HYPERION_UNIQUE_ID_HPP

#include <core/Defines.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {

namespace utilities {
namespace detail {

struct UniqueIDGenerator
{
    static HYP_API uint64 Generate();
};

} // namespace detail

struct UniqueID
{
    static inline uint64 DefaultValue()
    {
        return detail::UniqueIDGenerator::Generate();
    }

public:
    UniqueID()
        : value(DefaultValue())
    {
    }

    template <class T, typename = std::enable_if_t< !std::is_same_v< NormalizedType< T >, UniqueID > > >
    UniqueID(const T &value)
        : value(HashCode::GetHashCode(value).Value())
    {
    }

    UniqueID(const UniqueID &other)
        : value(other.value)
    {
    }

    UniqueID &operator=(const UniqueID &other)
    {
        value = other.value;

        return *this;
    }

    UniqueID(UniqueID &&other) noexcept
        : value(other.value)
    {
        other.value = DefaultValue();
    }

    UniqueID &operator=(UniqueID &&other) noexcept
    {
        value = other.value;
        other.value = DefaultValue();

        return *this;
    }

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
        UniqueID id;
        id.value = detail::UniqueIDGenerator::Generate();

        return id;
    }

private:
    uint64 value;
};

} // namespace utilities

using utilities::UniqueID;

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::UniqueID);

#endif