#ifndef TRIBOOL_HPP
#define TRIBOOL_HPP

#include <Types.hpp>

namespace hyperion {

class Tribool
{
public:
    enum TriboolValue : Int8
    {
        TRI_INDETERMINATE = -1,
        TRI_FALSE = 0,
        TRI_TRUE = 1
    };

    Tribool(TriboolValue value)
        : m_value(value)
    {
    }

    explicit Tribool(bool b)
        : m_value(b ? TRI_TRUE : TRI_FALSE)
    {
    }

    Tribool(const Tribool &other)
        : m_value(other.m_value)
    {
    }

    bool operator==(const Tribool &other) const
    {
        return m_value == other.m_value;
    }

    explicit operator bool() const = delete;

    operator int() const
    {
        return static_cast<int>(m_value);
    }

    static Tribool True()
    {
        return Tribool(TRI_TRUE);
    }

    static Tribool False()
    {
        return Tribool(TRI_FALSE);
    }

    static Tribool Indeterminate()
    {
        return Tribool(TRI_INDETERMINATE);
    }

private:
    TriboolValue m_value;
};

} // namespace hyperion

#endif
