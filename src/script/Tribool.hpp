#ifndef TRIBOOL_HPP
#define TRIBOOL_HPP

#include <Types.hpp>

namespace hyperion {

enum TriboolValue : Int8
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

    constexpr Tribool(const Tribool &other) = default;
    constexpr Tribool &operator=(const Tribool &other) = default;
    constexpr Tribool(Tribool &&other) noexcept = default;
    constexpr Tribool &operator=(Tribool &&other) noexcept = default;
    constexpr ~Tribool() = default;

    constexpr bool operator==(const Tribool &other) const
        { return m_value == other.m_value; }

    constexpr bool operator==(TriboolValue value) const
        { return m_value == value; }

    constexpr bool operator!=(const Tribool &other) const
        { return m_value != other.m_value; }

    constexpr bool operator!=(TriboolValue value) const
        { return m_value != value; }

    constexpr explicit operator Bool() const = delete;

    constexpr operator Int() const
        { return Int(m_value); }

    constexpr Int Value() const
        { return Int(m_value); }

    constexpr static Tribool True()
        { return Tribool(TRI_TRUE); }

    constexpr static Tribool False()
        { return Tribool(TRI_FALSE); }

    constexpr static Tribool Indeterminate()
        { return Tribool(TRI_INDETERMINATE); }

private:
    TriboolValue m_value;
};

} // namespace hyperion

#endif
