/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SEMAPHORE_HPP
#define HYPERION_SEMAPHORE_HPP

#include <core/Defines.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/functional/Proc.hpp>

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

    AtomicSemaphoreImpl(const AtomicSemaphoreImpl &other)                   = delete;
    AtomicSemaphoreImpl &operator=(const AtomicSemaphoreImpl &other)        = delete;

    AtomicSemaphoreImpl(AtomicSemaphoreImpl &&other) noexcept
        : value(other.value.Get(MemoryOrder::SEQUENTIAL))
    {
    }
    
    AtomicSemaphoreImpl &operator=(AtomicSemaphoreImpl &&other) noexcept    = delete;

    ~AtomicSemaphoreImpl()                                                  = default;

    void Acquire()
    {
        while (value.Get(MemoryOrder::SEQUENTIAL) > 0) {
            HYP_WAIT_IDLE();
        }
    }

    CounterType Release(CounterType delta = 1, ProcRef<void> if_signalled_proc = ProcRef<void>(nullptr))
    {
        CounterType value = value.Decrement(delta, MemoryOrder::SEQUENTIAL) - delta;

        if (value <= 0 && if_signalled_proc.IsValid()) {
            if_signalled_proc();
        }

        return value;
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

    ConditionVarSemaphoreImpl(const ConditionVarSemaphoreImpl &other)                   = delete;
    ConditionVarSemaphoreImpl &operator=(const ConditionVarSemaphoreImpl &other)        = delete;

    ConditionVarSemaphoreImpl(ConditionVarSemaphoreImpl &&other) noexcept
        : value(other.value)
    {
    }
    
    ConditionVarSemaphoreImpl &operator=(ConditionVarSemaphoreImpl &&other) noexcept    = delete;

    ~ConditionVarSemaphoreImpl()                                                        = default;

    void Acquire()
    {
        std::unique_lock<std::mutex> lock(mutex);

        while (value > 0) {
            cv.wait(lock);
        }
    }

    CounterType Release(CounterType delta = 1, ProcRef<void> if_signalled_proc = ProcRef<void>(nullptr))
    {
        std::lock_guard<std::mutex> lock(mutex);
        
        const CounterType previous_value = value;
        value -= delta;

        if (value <= 0 && if_signalled_proc.IsValid()) {
            if_signalled_proc();
        }

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

    Semaphore(const Semaphore &other)                   = delete;
    Semaphore &operator=(const Semaphore &other)        = delete;

    Semaphore(Semaphore &&other) noexcept
        : m_impl(std::move(other.m_impl))
    {
    }

    Semaphore &operator=(Semaphore &&other) noexcept    = delete;
    ~Semaphore()                                        = default;

    HYP_FORCE_INLINE void Acquire()
        { m_impl.Acquire(); }

    HYP_FORCE_INLINE CounterType Release(CounterType delta = 1, ProcRef<void> if_signalled_proc = ProcRef<void>(nullptr))
        { return m_impl.Release(delta, if_signalled_proc); }

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
