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

enum class SemaphoreDirection : uint8
{
    WAIT_FOR_ZERO_OR_NEGATIVE   = 0,
    WAIT_FOR_POSITIVE           = 1
};

namespace detail {

template <class T, SemaphoreDirection Direction = SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>
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
        if constexpr (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE) {
            while (value.Get(MemoryOrder::SEQUENTIAL) > 0) {
                HYP_WAIT_IDLE();
            }
        } else if constexpr (Direction == SemaphoreDirection::WAIT_FOR_POSITIVE) {
            while (value.Get(MemoryOrder::SEQUENTIAL) <= 0) {
                HYP_WAIT_IDLE();
            }
        } else {
            HYP_NOT_IMPLEMENTED_VOID();
        }
    }

    CounterType Release(CounterType delta = 1, ProcRef<void> if_signalled_proc = ProcRef<void>(nullptr))
    {
        CounterType current_value = value.Decrement(delta, MemoryOrder::SEQUENTIAL) - delta;
        
        if constexpr (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE) {
            if (current_value <= 0 && if_signalled_proc.IsValid()) {
                if_signalled_proc();
            }

            return current_value;
        } else if constexpr (Direction == SemaphoreDirection::WAIT_FOR_POSITIVE) {
            if (current_value > 0 && if_signalled_proc.IsValid()) {
                if_signalled_proc();
            }

            return current_value;
        } else {
            HYP_NOT_IMPLEMENTED();
        }
    }

    void Produce(CounterType delta = 1)
    {
        value.Increment(delta, MemoryOrder::SEQUENTIAL);
    }

    CounterType GetValue() const
    {
        return value.Get(MemoryOrder::SEQUENTIAL);
    }

    void SetValue(CounterType new_value)
    {
        value.Set(new_value, MemoryOrder::SEQUENTIAL);
    }

    bool IsInSignalState() const
    {
        if constexpr (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE) {
            return GetValue() <= 0;
        } else if constexpr (Direction == SemaphoreDirection::WAIT_FOR_POSITIVE) {
            return GetValue() > 0;
        } else {
            HYP_NOT_IMPLEMENTED();
        }
    }
};

template <class T, SemaphoreDirection Direction = SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>
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

        if constexpr (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE) {
            while (value > 0) {
                cv.wait(lock);
            }
        } else if constexpr (Direction == SemaphoreDirection::WAIT_FOR_POSITIVE) {
            while (value <= 0) {
                cv.wait(lock);
            }
        } else {
            HYP_NOT_IMPLEMENTED_VOID();
        }
    }

    CounterType Release(CounterType delta = 1, ProcRef<void> if_signalled_proc = ProcRef<void>(nullptr))
    {
        std::lock_guard<std::mutex> lock(mutex);

        const CounterType previous_value = value;
        value -= delta;

        if constexpr (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE) {
            if (value <= 0 && if_signalled_proc.IsValid()) {
                if_signalled_proc();
            }
        } else if constexpr (Direction == SemaphoreDirection::WAIT_FOR_POSITIVE) {
            if (value > 0 && if_signalled_proc.IsValid()) {
                if_signalled_proc();
            }
        } else {
            HYP_NOT_IMPLEMENTED();
        }

        cv.notify_all();

        return previous_value - delta;
    }

    void Produce(CounterType delta = 1)
    {
        std::lock_guard<std::mutex> lock(mutex);

        value += delta;

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

    bool IsInSignalState() const
    {
        if constexpr (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE) {
            return GetValue() <= 0;
        } else if constexpr (Direction == SemaphoreDirection::WAIT_FOR_POSITIVE) {
            return GetValue() > 0;
        } else {
            HYP_NOT_IMPLEMENTED();
        }
    }
};

} // namespace detail

class SemaphoreBase
{
public:
};

template <class CounterType, SemaphoreDirection Direction = SemaphoreDirection::WAIT_FOR_POSITIVE, class Impl = detail::ConditionVarSemaphoreImpl<CounterType, Direction>>
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
        { return m_impl.IsInSignalState(); }

private:
    Impl    m_impl;
};

} // namespace threading

using threading::Semaphore;
using threading::SemaphoreDirection;

} // namespace hyperion

#endif
