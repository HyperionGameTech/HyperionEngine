#pragma once

#include <core/Types.hpp>

namespace Hyperion {

enum TriboolValue : int8
{
    TRI_INDETERMINATE = -1,
    TRI_FALSE = 0,
    TRI_TRUE = 1
};

class Tribool
{
public:
    constexpr Tribool(TriboolValue value = TRI_FALSE)
        : m_value(value)
    {
    }

    constexpr explicit Tribool(bool b)
        : m_value(b ? TRI_TRUE : TRI_FALSE)
    {
    }

    constexpr Tribool(const Tribool& other) = default;
    constexpr Tribool& operator=(const Tribool& other) = default;
    constexpr Tribool(Tribool&& other) noexcept = default;
    constexpr Tribool& operator=(Tribool&& other) noexcept = default;
    constexpr ~Tribool() = default;

    constexpr bool operator==(const Tribool& other) const
    {
        return m_value == other.m_value;
    }

    constexpr bool operator==(TriboolValue value) const
    {
        return m_value == value;
    }

    constexpr bool operator!=(const Tribool& other) const
    {
        return m_value != other.m_value;
    }

    constexpr bool operator!=(TriboolValue value) const
    {
        return m_value != value;
    }

    constexpr explicit operator bool() const = delete;

    constexpr operator int() const
    {
        return int(m_value);
    }

    constexpr int Value() const
    {
        return int(m_value);
    }

    constexpr static Tribool True()
    {
        return Tribool(TRI_TRUE);
    }

    constexpr static Tribool False()
    {
        return Tribool(TRI_FALSE);
    }

    constexpr static Tribool Indeterminate()
    {
        return Tribool(TRI_INDETERMINATE);
    }

private:
    TriboolValue m_value;
};

} // namespace Hyperion
