/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/Constants.hpp>
#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <Core/utilities/ClockTimer.hpp>

namespace Hyperion {

template <class T = double>
class BlendVar
{
public:
    BlendVar()
        : m_value(T(0)),
          m_target(T(0)),
          m_fract(0.0),
          m_counter(0.33333) // 30fps
    {
    }

    explicit BlendVar(T value, double rate = 0.3333)
        : m_value(value),
          m_target(value),
          m_fract(0.0),
          m_counter(1.0 / MathUtil::Max(rate, 0.0001))
    {
    }

    BlendVar(T value, T target, double rate = 0.3333)
        : m_value(value),
          m_target(target),
          m_fract(0.0),
          m_counter(1.0 / MathUtil::Max(rate, 0.0001))
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
        m_fract = 0.0; // simply reset fraction when setting value directly
    }

    HYP_FORCE_INLINE T GetTarget() const
    {
        return m_target;
    }

    HYP_FORCE_INLINE void SetTarget(T target, bool resetBlending = false)
    {
        m_target = target;
        m_fract = 0.0;
    }

    /*! \brief Gets the rate of change in Hz (cycles per second).
     *  \return The rate of change in Hz.
     */
    HYP_FORCE_INLINE double GetRate() const
    {
        return 1.0 / m_counter.targetInterval;
    }

    /*! \brief Sets the rate of change in Hz (cycles per second).
     *  \param rate The rate of change in Hz.
     */
    HYP_FORCE_INLINE void SetRate(double rate)
    {
        m_counter = ClockTimer(1.0 / MathUtil::Max(rate, 0.0001));
    }

    /*! \brief Advances the blend variable towards the target value.
     *  \param outDelta The delta between the previous value and current value
     *  \return True if the blend variable has changed since the last advancement. False if the blend variable has reached the target value.
     */
    bool Advance(T& outDelta)
    {
        // done blending if fraction is >= 1.0
        if (m_fract >= 1.0)
        {
            return false;
        }

        if (m_counter.Waiting())
        {
            return true;
        }

        m_counter.NextTick();

        const double delta = m_counter.delta;

        const double fract = MathUtil::Clamp(m_fract + delta, 0.0, 1.0);

        T nextValue = MathUtil::Lerp(m_value, m_target, fract);

        outDelta = nextValue - m_value;

        const bool changed = !MathUtil::ApproxEqual(m_value, m_target);

        m_value = nextValue;
        m_fract = fract;

        return changed;
    }

    /*! \brief Advances the blend variable towards the target value.
     *  \return True if the blend variable has changed since the last advancement. False if the blend variable has reached the target value.
     */
    bool Advance()
    {
        T tmpDelta;

        return Advance(tmpDelta);
    }

private:
    T m_value;
    T m_target;
    double m_fract;
    ClockTimer m_counter;
};

} // namespace Hyperion
