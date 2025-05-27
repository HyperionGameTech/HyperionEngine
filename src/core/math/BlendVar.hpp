/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BLEND_VAR_HPP
#define HYPERION_BLEND_VAR_HPP

#include <core/Defines.hpp>

#include <core/math/MathUtil.hpp>

#include <Constants.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {

template <class T = double>
class BlendVar
{
public:
    BlendVar() = default;

    explicit BlendVar(T value)
        : m_value(value),
          m_target(value),
          m_fract(0.0)
    {
    }

    BlendVar(T value, T target)
        : m_value(value),
          m_target(target),
          m_fract(0.0)
    {
    }

    BlendVar(const BlendVar& other) = default;
    BlendVar& operator=(const BlendVar& other) = default;
    BlendVar(BlendVar&& other) noexcept = default;
    BlendVar& operator=(BlendVar&& other) noexcept = default;
    ~BlendVar() = default;

    HYP_FORCE_INLINE T GetValue() const
    {
        return m_value;
    }

    HYP_FORCE_INLINE void SetValue(T value)
    {
        m_value = value;
        m_fract = 0.0;
    }

    HYP_FORCE_INLINE T GetTarget() const
    {
        return m_target;
    }

    HYP_FORCE_INLINE void SetTarget(T target)
    {
        m_target = target;
        m_fract = 0.0;
    }

    /*! \brief Advances the blend variable towards the target value.
     *  \param delta The amount to advance the blend variable.
     *  \param out_delta The delta between the previous value and current value
     *  \return True if the blend variable has changed since the last advancement. False if the blend variable has reached the target value.
     */
    bool Advance(double delta, T& out_delta)
    {
        m_fract = MathUtil::Clamp(m_fract + delta, 0.0, 1.0);

        T next_value = MathUtil::Lerp(m_value, m_target, m_fract);

        out_delta = next_value - m_value;

        // T next_value = m_value + (m_target - m_value) * T(std::log(1.0 + 25.0 * m_fract) / std::log(1.0 + 25.0));
        const bool changed = m_value != next_value; //! MathUtil::ApproxEqual(m_value, m_target);

        m_value = next_value;

        return changed;
    }

    /*! \brief Advances the blend variable towards the target value.
     *  \param delta The amount to advance the blend variable.
     *  \return True if the blend variable has changed since the last advancement. False if the blend variable has reached the target value.
     */
    bool Advance(double delta)
    {
        T tmp_delta;

        return Advance(delta, tmp_delta);
    }

private:
    T m_value {};
    T m_target {};
    double m_fract = 0.0;
};

} // namespace hyperion

#endif