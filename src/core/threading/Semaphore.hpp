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
    WAIT_FOR_ZERO_OR_NEGATIVE   = 0,
    WAIT_FOR_POSITIVE           = 1
};

namespace detail {

template <class T, SemaphoreDirection Direction>
static inline constexpr bool ShouldSignal(T value)
{
    if constexpr (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE) {
        return value <= 0;
    } else if constexpr (Direction == SemaphoreDirection::WAIT_FOR_POSITIVE) {
        return value > 0;
    }

    return false;
}

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

    CounterType Release(CounterType delta = 1, ProcRef<void, bool> if_signal_state_changed_proc = ProcRef<void, bool>(nullptr))
    {
        CounterType previous_value = value.Decrement(delta, MemoryOrder::SEQUENTIAL);
        CounterType current_value = previous_value - delta;

        if (if_signal_state_changed_proc.IsValid()) {
            const bool should_signal_before = ShouldSignal<CounterType, Direction>(previous_value);
            const bool should_signal_after = ShouldSignal<CounterType, Direction>(current_value);
            
            if (should_signal_before != should_signal_after) {
                if_signal_state_changed_proc(should_signal_after);
            }
        }

        return current_value;
    }

    CounterType Release(CounterType delta = 1, ProcRef<void> if_signalled_proc = ProcRef<void>(nullptr))
    {
        CounterType previous_value = value.Decrement(delta, MemoryOrder::SEQUENTIAL);
        CounterType current_value = previous_value - delta;

        if (if_signalled_proc.IsValid() && ShouldSignal<CounterType, Direction>(current_value)) {
            if_signalled_proc();
        }

        return current_value;
    }

    void Produce(CounterType delta = 1, ProcRef<void, bool> if_signal_state_changed_proc = ProcRef<void, bool>(nullptr))
    {
        CounterType previous_value = value.Increment(delta, MemoryOrder::SEQUENTIAL);
        CounterType current_value = previous_value + delta;

        if (if_signal_state_changed_proc.IsValid()) {
            const bool should_signal_before = ShouldSignal<CounterType, Direction>(previous_value);
            const bool should_signal_after = ShouldSignal<CounterType, Direction>(current_value);
            
            if (should_signal_before != should_signal_after) {
                if_signal_state_changed_proc(should_signal_after);
            }
        }
    }

    void Produce(CounterType delta = 1, ProcRef<void> if_signalled_proc = ProcRef<void>(nullptr))
    {
        CounterType previous_value = value.Increment(delta, MemoryOrder::SEQUENTIAL);
        CounterType current_value = previous_value + delta;

        if (if_signalled_proc.IsValid() && ShouldSignal<CounterType, Direction>(current_value)) {
            if_signalled_proc();
        }
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
        return ShouldSignal<CounterType, Direction>(GetValue());
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

    CounterType Release(CounterType delta = 1, ProcRef<void, bool> if_signal_state_changed_proc = ProcRef<void, bool>(nullptr))
    {
        std::lock_guard<std::mutex> lock(mutex);

        const CounterType previous_value = value;
        value -= delta;

        if (if_signal_state_changed_proc.IsValid()) {
            const bool should_signal_before = ShouldSignal<CounterType, Direction>(previous_value);
            const bool should_signal_after = ShouldSignal<CounterType, Direction>(value);

            if (should_signal_before != should_signal_after) {
                if_signal_state_changed_proc(should_signal_after);
            }
        }

        cv.notify_all();

        return previous_value - delta;
    }

    CounterType Release(CounterType delta = 1, ProcRef<void> if_signalled_proc = ProcRef<void>(nullptr))
    {
        std::lock_guard<std::mutex> lock(mutex);

        const CounterType previous_value = value;
        value -= delta;

        if (if_signalled_proc.IsValid() && ShouldSignal<CounterType, Direction>(value)) {
            if_signalled_proc();
        }

        cv.notify_all();

        return previous_value - delta;
    }

    void Produce(CounterType delta = 1, ProcRef<void, bool> if_signal_state_changed_proc = ProcRef<void, bool>(nullptr))
    {
        std::lock_guard<std::mutex> lock(mutex);

        const CounterType previous_value = value;
        value += delta;

        if (if_signal_state_changed_proc.IsValid()) {
            const bool should_signal_before = ShouldSignal<CounterType, Direction>(previous_value);
            const bool should_signal_after = ShouldSignal<CounterType, Direction>(value);

            if (should_signal_before != should_signal_after) {
                if_signal_state_changed_proc(should_signal_after);
            }
        }

        cv.notify_all();
    }

    void Produce(CounterType delta = 1, ProcRef<void> if_signalled_proc = ProcRef<void>(nullptr))
    {
        std::lock_guard<std::mutex> lock(mutex);

        const CounterType previous_value = value;
        value += delta;

        if (if_signalled_proc.IsValid() && ShouldSignal<CounterType, Direction>(value)) {
            if_signalled_proc();
        }

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
        return ShouldSignal<CounterType, Direction>(GetValue());
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

    HYP_FORCE_INLINE CounterType Release(CounterType delta, ProcRef<void, bool> if_signal_state_changed_proc)
        { return m_impl.Release(delta, if_signal_state_changed_proc); }

    HYP_FORCE_INLINE CounterType Release(CounterType delta, ProcRef<void> if_signalled_proc)
        { return m_impl.Release(delta, if_signalled_proc); }

    HYP_FORCE_INLINE CounterType Release(CounterType delta = 1)
        { return m_impl.Release(delta, ProcRef<void>(nullptr)); }

    HYP_FORCE_INLINE void Produce(CounterType increment, ProcRef<void, bool> if_signal_state_changed_proc)
        { m_impl.Produce(increment, if_signal_state_changed_proc); }

    HYP_FORCE_INLINE void Produce(CounterType increment, ProcRef<void> if_signalled_proc)
        { m_impl.Produce(increment, if_signalled_proc); }

    HYP_FORCE_INLINE void Produce(CounterType increment = 1)
        { m_impl.Produce(increment, ProcRef<void>(nullptr)); }

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
