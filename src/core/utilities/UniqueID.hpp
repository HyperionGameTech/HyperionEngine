/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UNIQUE_ID_HPP
#define HYPERION_UNIQUE_ID_HPP

#include <core/Defines.hpp>

#include <core/utilities/UUID.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {
namespace utilities {

class UniqueID
{
public:
    UniqueID()
        : m_value(Generate().m_value)
    {
    }

    explicit constexpr UniqueID(const HashCode& hash_code)
        : m_value(hash_code.Value())
    {
    }

    explicit constexpr UniqueID(uint64 value)
        : m_value(value)
    {
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<NormalizedType<T>, UniqueID> && HYP_HAS_METHOD(T, GetHashCode)>>
    explicit UniqueID(const T& value)
        : m_value(HashCode::GetHashCode(value).Value())
    {
    }

    UniqueID(const UniqueID& other) = default;
    UniqueID& operator=(const UniqueID& other) = default;
    UniqueID(UniqueID&& other) noexcept = default;
    UniqueID& operator=(UniqueID&& other) noexcept = default;

    HYP_FORCE_INLINE constexpr bool operator==(const UniqueID& other) const
    {
        return m_value == other.m_value;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const UniqueID& other) const
    {
        return m_value != other.m_value;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const UniqueID& other) const
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

    static inline UniqueID Generate()
    {
        return UniqueID { UUID {}.GetHashCode() };
    }

    static inline constexpr UniqueID Invalid()
    {
        return UniqueID { 0 };
    }

    static UniqueID FromHashCode(const HashCode& hash_code)
    {
        return UniqueID { hash_code };
    }

    static UniqueID FromUUID(const UUID& uuid)
    {
        return UniqueID { HashCode::GetHashCode(uuid.data0).Combine(HashCode::GetHashCode(uuid.data1)) };
    }

private:
    uint64 m_value;
};

} // namespace utilities

using utilities::UniqueID;

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::UniqueID);

#endif