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

#define HYP_SIGN(value) (int(0 < int(value)) - int((value) < 0))

namespace hyperion {
namespace threading {

enum class SemaphoreDirection : uint8
{
    WAIT_FOR_ZERO_OR_NEGATIVE = 0,
    WAIT_FOR_POSITIVE = 1
};

template <class T, SemaphoreDirection Direction>
static inline constexpr bool ShouldSignal(T value)
{
    if constexpr (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE)
    {
        return value <= 0;
    }
    else if constexpr (Direction == SemaphoreDirection::WAIT_FOR_POSITIVE)
    {
        return value > 0;
    }

    return false;
}

template <class T, SemaphoreDirection Direction = SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>
struct AtomicSemaphoreImpl
{
    using CounterType = T;

    mutable AtomicVar<CounterType> value;

    AtomicSemaphoreImpl(CounterType initial_value)
        : value(initial_value)
    {
    }

    AtomicSemaphoreImpl(const AtomicSemaphoreImpl& other) = delete;
    AtomicSemaphoreImpl& operator=(const AtomicSemaphoreImpl& other) = delete;

    AtomicSemaphoreImpl(AtomicSemaphoreImpl&& other) noexcept
        : value(other.value.Exchange(0, MemoryOrder::ACQUIRE_RELEASE))
    {
    }

    AtomicSemaphoreImpl& operator=(AtomicSemaphoreImpl&& other) noexcept = delete;

    ~AtomicSemaphoreImpl() = default;

    void Acquire() const
    {
        if constexpr (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE)
        {
            while (value.Get(MemoryOrder::ACQUIRE) > 0)
            {
                HYP_WAIT_IDLE();
            }
        }
        else if constexpr (Direction == SemaphoreDirection::WAIT_FOR_POSITIVE)
        {
            while (value.Get(MemoryOrder::ACQUIRE) <= 0)
            {
                HYP_WAIT_IDLE();
            }
        }
        else
        {
            HYP_NOT_IMPLEMENTED_VOID();
        }
    }

    void Acquire(ProcRef<void()> callback) const
    {
        // Call `callback` when the semaphore is acquired using Compare-Exchange
        // When the callback has been called, we can revert back to the previous state
        while (true)
        {
            CounterType current_value = value.Get(MemoryOrder::ACQUIRE);

            if (ShouldSignal<CounterType, Direction>(current_value))
            {
                if (value.CompareExchangeWeak(current_value, current_value + (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE ? 1 : -1), MemoryOrder::ACQUIRE_RELEASE))
                {
                    if (callback.IsValid())
                    {
                        callback();
                    }

                    // Go back to the previous state
                    value.Decrement(Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE ? 1 : -1, MemoryOrder::RELEASE);

                    break;
                }
            }
        }
    }

    CounterType Release(CounterType delta = 1, ProcRef<void(bool)> if_signal_state_changed_proc = ProcRef<void(bool)>(nullptr))
    {
        CounterType previous_value = value.Decrement(delta, MemoryOrder::ACQUIRE_RELEASE);
        CounterType current_value = previous_value - delta;

        if (if_signal_state_changed_proc.IsValid())
        {
            const bool should_signal_before = ShouldSignal<CounterType, Direction>(previous_value);
            const bool should_signal_after = ShouldSignal<CounterType, Direction>(current_value);

            if (should_signal_before != should_signal_after)
            {
                if_signal_state_changed_proc(should_signal_after);
            }
        }

        return current_value;
    }

    CounterType Release(CounterType delta = 1, ProcRef<void()> if_signalled_proc = ProcRef<void()>(nullptr))
    {
        CounterType previous_value = value.Decrement(delta, MemoryOrder::ACQUIRE_RELEASE);
        CounterType current_value = previous_value - delta;

        if (if_signalled_proc.IsValid() && ShouldSignal<CounterType, Direction>(current_value))
        {
            if_signalled_proc();
        }

        return current_value;
    }

    CounterType Produce(CounterType delta = 1, ProcRef<void(bool)> if_signal_state_changed_proc = ProcRef<void(bool)>(nullptr))
    {
        CounterType previous_value = value.Increment(delta, MemoryOrder::ACQUIRE_RELEASE);
        CounterType current_value = previous_value + delta;

        if (if_signal_state_changed_proc.IsValid())
        {
            const bool should_signal_before = ShouldSignal<CounterType, Direction>(previous_value);
            const bool should_signal_after = ShouldSignal<CounterType, Direction>(current_value);

            if (should_signal_before != should_signal_after)
            {
                if_signal_state_changed_proc(should_signal_after);
            }
        }

        return current_value;
    }

    CounterType Produce(CounterType delta = 1, ProcRef<void()> if_signalled_proc = ProcRef<void()>(nullptr))
    {
        CounterType previous_value = value.Increment(delta, MemoryOrder::ACQUIRE_RELEASE);
        CounterType current_value = previous_value + delta;

        if (if_signalled_proc.IsValid() && ShouldSignal<CounterType, Direction>(current_value))
        {
            if_signalled_proc();
        }

        return current_value;
    }

    void WaitForValue(T target_value) const
    {
        if constexpr (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE)
        {
            while (value.Get(MemoryOrder::ACQUIRE) >= target_value)
            {
                HYP_WAIT_IDLE();
            }
        }
        else if constexpr (Direction == SemaphoreDirection::WAIT_FOR_POSITIVE)
        {
            while (value.Get(MemoryOrder::ACQUIRE) <= target_value)
            {
                HYP_WAIT_IDLE();
            }
        }
        else
        {
            HYP_NOT_IMPLEMENTED_VOID();
        }
    }

    CounterType GetValue() const
    {
        return value.Get(MemoryOrder::ACQUIRE);
    }

    void SetValue(CounterType new_value)
    {
        value.Set(new_value, MemoryOrder::RELEASE);
    }

    bool IsInSignalState() const
    {
        return ShouldSignal<CounterType, Direction>(GetValue());
    }
};

template <class T, SemaphoreDirection Direction = SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>
struct ConditionVarSemaphoreImpl
{
    using CounterType = T;

    mutable std::mutex mutex;
    mutable std::condition_variable cv;
    mutable AtomicVar<CounterType> value;

    ConditionVarSemaphoreImpl(CounterType initial_value)
        : value(initial_value)
    {
    }

    ConditionVarSemaphoreImpl(const ConditionVarSemaphoreImpl& other) = delete;
    ConditionVarSemaphoreImpl& operator=(const ConditionVarSemaphoreImpl& other) = delete;

    ConditionVarSemaphoreImpl(ConditionVarSemaphoreImpl&& other) noexcept
        : value(other.value.Exchange(0, MemoryOrder::ACQUIRE_RELEASE))
    {
    }

    ConditionVarSemaphoreImpl& operator=(ConditionVarSemaphoreImpl&& other) noexcept = delete;

    ~ConditionVarSemaphoreImpl() = default;

    void Acquire() const
    {
        std::unique_lock<std::mutex> lock(mutex);

        if constexpr (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE)
        {
            while (value.Get(MemoryOrder::ACQUIRE) > 0)
            {
                cv.wait(lock);
            }
        }
        else if constexpr (Direction == SemaphoreDirection::WAIT_FOR_POSITIVE)
        {
            while (value.Get(MemoryOrder::ACQUIRE) <= 0)
            {
                cv.wait(lock);
            }
        }
        else
        {
            HYP_NOT_IMPLEMENTED_VOID();
        }
    }

    void Acquire(ProcRef<void()> callback) const
    {
        std::unique_lock<std::mutex> lock(mutex);

        while (true)
        {
            const CounterType current_value = value.Get(MemoryOrder::ACQUIRE);

            if (ShouldSignal<CounterType, Direction>(current_value))
            {
                if (value.CompareExchangeWeak(current_value, current_value + (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE ? 1 : -1), MemoryOrder::ACQUIRE_RELEASE))
                {
                    if (callback.IsValid())
                    {
                        callback();
                    }

                    value.Decrement(Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE ? 1 : -1, MemoryOrder::RELEASE);

                    break;
                }
            }

            cv.wait(lock);
        }
    }

    CounterType Release(CounterType delta = 1, ProcRef<void(bool)> if_signal_state_changed_proc = ProcRef<void(bool)>(nullptr))
    {
        std::lock_guard<std::mutex> lock(mutex);

        const CounterType previous_value = value.Decrement(delta, MemoryOrder::ACQUIRE_RELEASE);
        const CounterType new_value = previous_value - delta;

        if (if_signal_state_changed_proc.IsValid())
        {
            const bool should_signal_before = ShouldSignal<CounterType, Direction>(previous_value);
            const bool should_signal_after = ShouldSignal<CounterType, Direction>(new_value);

            if (should_signal_before != should_signal_after)
            {
                if_signal_state_changed_proc(should_signal_after);
            }
        }

        cv.notify_all();

        return new_value;
    }

    CounterType Release(CounterType delta = 1, ProcRef<void()> if_signalled_proc = ProcRef<void()>(nullptr))
    {
        std::lock_guard<std::mutex> lock(mutex);

        const CounterType new_value = value.Decrement(delta, MemoryOrder::ACQUIRE_RELEASE) - delta;

        if (if_signalled_proc.IsValid() && ShouldSignal<CounterType, Direction>(new_value))
        {
            if_signalled_proc();
        }

        cv.notify_all();

        return new_value;
    }

    CounterType Produce(CounterType delta = 1, ProcRef<void(bool)> if_signal_state_changed_proc = ProcRef<void(bool)>(nullptr))
    {
        std::lock_guard<std::mutex> lock(mutex);

        const CounterType previous_value = value.Increment(delta, MemoryOrder::ACQUIRE_RELEASE);
        const CounterType new_value = previous_value + delta;

        if (if_signal_state_changed_proc.IsValid())
        {
            const bool should_signal_before = ShouldSignal<CounterType, Direction>(previous_value);
            const bool should_signal_after = ShouldSignal<CounterType, Direction>(new_value);

            if (should_signal_before != should_signal_after)
            {
                if_signal_state_changed_proc(should_signal_after);
            }
        }

        cv.notify_all();

        return new_value;
    }

    CounterType Produce(CounterType delta = 1, ProcRef<void()> if_signalled_proc = ProcRef<void()>(nullptr))
    {
        std::lock_guard<std::mutex> lock(mutex);

        const CounterType new_value = value.Increment(delta, MemoryOrder::ACQUIRE_RELEASE) + delta;

        if (if_signalled_proc.IsValid() && ShouldSignal<CounterType, Direction>(new_value))
        {
            if_signalled_proc();
        }

        cv.notify_all();

        return new_value;
    }

    void WaitForValue(T target_value) const
    {
        std::unique_lock<std::mutex> lock(mutex);

        if constexpr (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE)
        {
            while (value.Get(MemoryOrder::ACQUIRE) >= target_value)
            {
                cv.wait(lock);
            }
        }
        else if constexpr (Direction == SemaphoreDirection::WAIT_FOR_POSITIVE)
        {
            while (value.Get(MemoryOrder::ACQUIRE) <= target_value)
            {
                cv.wait(lock);
            }
        }
        else
        {
            HYP_NOT_IMPLEMENTED_VOID();
        }
    }

    CounterType GetValue() const
    {
        return value.Get(MemoryOrder::ACQUIRE);
    }

    void SetValue(CounterType new_value)
    {
        std::lock_guard<std::mutex> lock(mutex);
        value.Set(new_value, MemoryOrder::RELEASE);
        cv.notify_all();
    }

    bool IsInSignalState() const
    {
        return ShouldSignal<CounterType, Direction>(GetValue());
    }
};

class SemaphoreBase
{
public:
};

template <class CounterType, SemaphoreDirection Direction = SemaphoreDirection::WAIT_FOR_POSITIVE, class Impl = ConditionVarSemaphoreImpl<CounterType, Direction>>
class Semaphore : public SemaphoreBase
{
public:
    struct Guard
    {
        Semaphore& m_semaphore;

        Guard(Semaphore& semaphore)
            : m_semaphore(semaphore)
        {
            m_semaphore.Produce();
        }

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

        Guard(Guard&& other) noexcept = delete;
        Guard& operator=(Guard&& other) noexcept = delete;

        ~Guard()
        {
            m_semaphore.Release();
        }
    };

    Semaphore(CounterType initial_value = 0)
        : m_impl(initial_value)
    {
    }

    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;

    Semaphore(Semaphore&& other) noexcept
        : m_impl(std::move(other.m_impl))
    {
    }

    Semaphore& operator=(Semaphore&& other) noexcept = delete;
    ~Semaphore() = default;

    HYP_FORCE_INLINE void Acquire() const
    {
        m_impl.Acquire();
    }

    // Do something when the semaphore is acquired
    HYP_FORCE_INLINE void Acquire(ProcRef<void()> callback) const
    {
        m_impl.Acquire(callback);
    }

    HYP_FORCE_INLINE CounterType Release(CounterType delta, ProcRef<void(bool)> if_signal_state_changed_proc)
    {
        return m_impl.Release(delta, if_signal_state_changed_proc);
    }

    HYP_FORCE_INLINE CounterType Release(CounterType delta, ProcRef<void()> if_signalled_proc)
    {
        return m_impl.Release(delta, if_signalled_proc);
    }

    HYP_FORCE_INLINE CounterType Release(CounterType delta = 1)
    {
        return m_impl.Release(delta, ProcRef<void()>(nullptr));
    }

    HYP_FORCE_INLINE CounterType Produce(CounterType increment, ProcRef<void(bool)> if_signal_state_changed_proc)
    {
        return m_impl.Produce(increment, if_signal_state_changed_proc);
    }

    HYP_FORCE_INLINE CounterType Produce(CounterType increment, ProcRef<void()> if_signalled_proc)
    {
        return m_impl.Produce(increment, if_signalled_proc);
    }

    HYP_FORCE_INLINE CounterType Produce(CounterType increment = 1)
    {
        return m_impl.Produce(increment, ProcRef<void()>(nullptr));
    }

    void WaitForValue(CounterType target_value) const
    {
        m_impl.WaitForValue(target_value);
    }

    HYP_FORCE_INLINE CounterType GetValue() const
    {
        return m_impl.GetValue();
    }

    HYP_FORCE_INLINE void SetValue(CounterType value)
    {
        m_impl.SetValue(value);
    }

    HYP_FORCE_INLINE bool IsInSignalState() const
    {
        return m_impl.IsInSignalState();
    }

private:
    Impl m_impl;
};

} // namespace threading

using threading::Semaphore;
using threading::SemaphoreDirection;

} // namespace hyperion

#endif
