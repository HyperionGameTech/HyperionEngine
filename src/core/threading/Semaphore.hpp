/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SEMAPHORE_HPP
#define HYPERION_SEMAPHORE_HPP

#include <core/Defines.hpp>
#include <core/threading/AtomicVar.hpp>

#include <Types.hpp>

#include <mutex>
#include <condition_variable>
#include <type_traits>

namespace hyperion {
namespace threading {

namespace detail {

template <class T>
struct AtomicSemaphoreImpl
{
    using CounterType = T;

    static_assert(std::is_signed_v<CounterType>, "CounterType must be a signed type as value can go below 0");

    AtomicVar<CounterType>  value;

    AtomicSemaphoreImpl(CounterType initial_value)
        : value(initial_value)
    {
    }

    void Acquire()
    {
        while (value.Get(MemoryOrder::SEQUENTIAL) > 0) {
            HYP_WAIT_IDLE();
        }
    }

    CounterType Release(CounterType delta = 1)
    {
        return value.Decrement(delta, MemoryOrder::SEQUENTIAL) - delta;
    }

    void Produce(CounterType increment = 1)
    {
        value.Increment(increment, MemoryOrder::SEQUENTIAL);
    }

    CounterType GetValue() const
    {
        return value.Get(MemoryOrder::SEQUENTIAL);
    }

    void SetValue(CounterType new_value)
    {
        value.Set(new_value, MemoryOrder::SEQUENTIAL);
    }
};

// @FIXME 
template <class T>
struct ConditionVarSemaphoreImpl
{
    using CounterType = T;

    static_assert(std::is_signed_v<CounterType>, "CounterType must be a signed type as value can go below 0");

    mutable std::mutex      mutex;
    std::condition_variable cv;
    CounterType             value;

    ConditionVarSemaphoreImpl(CounterType initial_value)
        : value(initial_value)
    {
    }

    void Acquire()
    {
        std::unique_lock<std::mutex> lock(mutex);
        while (value > 0) {
            cv.wait(lock);
        }
    }

    CounterType Release(CounterType delta = 1)
    {
        std::lock_guard<std::mutex> lock(mutex);
        const CounterType previous_value = value;
        value -= delta;
        cv.notify_all();
        return previous_value - delta;
    }

    void Produce(CounterType increment = 1)
    {
        std::lock_guard<std::mutex> lock(mutex);
        value += increment;
        cv.notify_all();
    }

    CounterType GetValue() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return value;
    }

    void SetValue(CounterType new_value)
    {
        std::lock_guard<std::mutex> lock(mutex);
        value = new_value;
        cv.notify_all();
    }
};

} // namespace detail

class SemaphoreBase
{
public:
};

template <class CounterType, class Impl = detail::ConditionVarSemaphoreImpl<CounterType>>
class Semaphore : public SemaphoreBase
{
public:
    Semaphore(CounterType initial_value = 0)
        : m_impl(initial_value)
    {
    }

    Semaphore(const Semaphore &other) = delete;
    Semaphore &operator=(const Semaphore &other) = delete;
    Semaphore(Semaphore &&other) = delete;
    Semaphore &operator=(Semaphore &&other) = delete;
    ~Semaphore() = default;

    HYP_FORCE_INLINE void Acquire()
        { m_impl.Acquire();  }

    HYP_FORCE_INLINE CounterType Release(CounterType delta = 1)
        { return m_impl.Release(delta); }

    HYP_FORCE_INLINE void Produce(CounterType increment = 1)
        { m_impl.Produce(increment); }

    HYP_FORCE_INLINE CounterType GetValue() const
        { return m_impl.GetValue(); }

    HYP_FORCE_INLINE void SetValue(CounterType value)
        { m_impl.SetValue(value); }

    HYP_FORCE_INLINE bool IsInSignalState() const
        { return m_impl.GetValue() <= 0; }

private:
    Impl    m_impl;
};

} // namespace threading

using threading::Semaphore;

} // namespace hyperion

#endif
