/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/utilities/Uuid.hpp>

#include <core/Types.hpp>
#include <core/HashCode.hpp>

namespace hyperion {
namespace utilities {

class UniqueId
{
public:
    UniqueId()
        : m_value(Generate().m_value)
    {
    }

    explicit constexpr UniqueId(const HashCode& hashCode)
        : m_value(hashCode.Value())
    {
    }

    explicit constexpr UniqueId(uint64 value)
        : m_value(value)
    {
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<NormalizedType<T>, UniqueId> && HYP_HAS_METHOD(T, GetHashCode)>>
    explicit UniqueId(const T& value)
        : m_value(HashCode::GetHashCode(value).Value())
    {
    }

    UniqueId(const UniqueId& other) = default;
    UniqueId& operator=(const UniqueId& other) = default;
    UniqueId(UniqueId&& other) noexcept = default;
    UniqueId& operator=(UniqueId&& other) noexcept = default;

    HYP_FORCE_INLINE constexpr bool operator==(const UniqueId& other) const
    {
        return m_value == other.m_value;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const UniqueId& other) const
    {
        return m_value != other.m_value;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const UniqueId& other) const
    {
        return m_value < other.m_value;
    }

    HYP_FORCE_INLINE constexpr operator uint64() const
    {
        return m_value;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode(m_value);
    }

    HYP_FORCE_INLINE ANSIString ToString() const
    {
        return ANSIString::ToString(m_value);
    }

    static inline UniqueId Generate()
    {
        return UniqueId { Uuid {}.GetHashCode() };
    }

    static inline constexpr UniqueId Invalid()
    {
        return UniqueId { 0 };
    }

    static UniqueId FromHashCode(const HashCode& hashCode)
    {
        return UniqueId { hashCode };
    }

    static UniqueId FromUUID(const Uuid& uuid)
    {
        return UniqueId { HashCode::GetHashCode(uuid.data0).Combine(HashCode::GetHashCode(uuid.data1)) };
    }

private:
    uint64 m_value;
};

} // namespace utilities

using utilities::UniqueId;

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::UniqueId);
