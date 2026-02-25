/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

namespace Hyperion {
namespace utilities {

struct UUID;

class HYP_API UniqueId
{
public:
    UniqueId();

    explicit UniqueId(const HashCode& hashCode)
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

    static UniqueId Generate();

    static inline constexpr UniqueId Invalid()
    {
        return UniqueId { 0 };
    }

    static UniqueId FromHashCode(const HashCode& hashCode)
    {
        return UniqueId { hashCode };
    }

    static UniqueId FromUUID(const UUID& uuid);

private:
    uint64 m_value;
};

} // namespace utilities

using utilities::UniqueId;

} // namespace Hyperion

HYP_DEF_STL_HASH(Hyperion::UniqueId);
