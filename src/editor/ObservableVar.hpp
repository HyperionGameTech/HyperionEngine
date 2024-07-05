/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_OBSERVABLE_VAR_HPP
#define HYPERION_OBSERVABLE_VAR_HPP

#include <core/functional/Delegate.hpp>

namespace hyperion {

template <class T>
class ObservableVar
{
public:
    ObservableVar() = default;

    ObservableVar(const T &value)
        : m_value(value)
    {
    }

    ObservableVar(const ObservableVar &other)                   = delete;
    ObservableVar &operator=(const ObservableVar &other)        = delete;

    ObservableVar(ObservableVar &&other) noexcept
        : m_value(std::move(other.m_value)),
          m_on_change(std::move(other.m_on_change))
    {
    }

    ObservableVar &operator=(ObservableVar &&other) noexcept    = delete;

    const T &Get() const
    {
        return m_value;
    }

    void Set(const T &value)
    {
        if (m_value != value) {
            m_value = value;
            m_on_change(m_value);
        }
    }

private:
    T                           m_value;
    Delegate<void, const T &>   m_on_change;
};
} // namespace hyperion

#endif
