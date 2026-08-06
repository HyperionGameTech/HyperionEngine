/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Threading/Mutex.hpp>

#include <utility>

namespace Hyperion {
namespace threading {

template <class T>
class Guarded
{
public:
    Guarded() = default;

    explicit Guarded(T value)
        : m_value(std::move(value))
    {
    }

    Guarded(const Guarded& other) = delete;
    Guarded& operator=(const Guarded& other) = delete;

    Guarded(Guarded&& other) noexcept = delete;
    Guarded& operator=(Guarded&& other) noexcept = delete;

    ~Guarded() = default;

    HYP_FORCE_INLINE T Get() const
    {
        Mutex::Guard guard(m_mutex);

        return m_value;
    }

    HYP_FORCE_INLINE void Set(T value)
    {
        T previousValue;

        {
            Mutex::Guard guard(m_mutex);

            previousValue = std::move(m_value);
            m_value = std::move(value);
        }
    }

    template <class Function>
    HYP_FORCE_INLINE decltype(auto) Access(Function&& fn)
    {
        Mutex::Guard guard(m_mutex);

        return fn(m_value);
    }

    template <class Function>
    HYP_FORCE_INLINE decltype(auto) Access(Function&& fn) const
    {
        Mutex::Guard guard(m_mutex);

        return fn(m_value);
    }

private:
    mutable Mutex m_mutex;
    T m_value;
};

} // namespace threading

using threading::Guarded;

} // namespace Hyperion
