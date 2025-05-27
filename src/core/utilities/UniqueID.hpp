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
        : m_value(Generate().m_value)
    {
    }

    UniqueID(const HashCode& hash_code)
        : m_value(hash_code.Value())
    {
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<NormalizedType<T>, UniqueID>>>
    UniqueID(const T& value)
        : m_value(HashCode::GetHashCode(value).Value())
    {
    }

    UniqueID(const UniqueID& other) = default;
    UniqueID& operator=(const UniqueID& other) = default;
    UniqueID(UniqueID&& other) noexcept = default;
    UniqueID& operator=(UniqueID&& other) noexcept = default;

    bool operator==(const UniqueID& other) const
    {
        return m_value == other.m_value;
    }

    bool operator!=(const UniqueID& other) const
    {
        return m_value != other.m_value;
    }

    bool operator<(const UniqueID& other) const
    {
        return m_value < other.m_value;
    }

    operator uint64() const
    {
        return m_value;
    }

    HashCode GetHashCode() const
    {
        return HashCode(m_value);
    }

    static inline UniqueID Generate()
    {
        return { UUID {} };
    }

    static inline UniqueID Invalid()
    {
        return { 0 };
    }

private:
    uint64 m_value;
};

} // namespace utilities

using utilities::UniqueID;

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::UniqueID);

#endif