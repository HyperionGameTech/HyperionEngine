/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/threading/AtomicVar.hpp>

#include <Core/functional/Proc.hpp>

#include <Core/Types.hpp>

#include <mutex>
#include <condition_variable>
#include <type_traits>

#define HYP_SIGN(value) (int(0 < int(value)) - int((value) < 0))

namespace Hyperion {
namespace threading {

HYP_API extern void ThreadSleep(uint32 milliseconds);

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

    AtomicSemaphoreImpl(CounterType initialValue)
        : value(initialValue)
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
            CounterType currentValue = value.Get(MemoryOrder::ACQUIRE);

            if (ShouldSignal<CounterType, Direction>(currentValue))
            {
                if (value.CompareExchangeWeak(currentValue, currentValue + (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE ? 1 : -1), MemoryOrder::ACQUIRE_RELEASE))
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

    CounterType Release(CounterType delta = 1, ProcRef<void(bool)> ifSignalStateChangedProc = ProcRef<void(bool)>(nullptr))
    {
        CounterType previousValue = value.Decrement(delta, MemoryOrder::ACQUIRE_RELEASE);
        CounterType currentValue = previousValue - delta;

        if (ifSignalStateChangedProc.IsValid())
        {
            const bool shouldSignalBefore = ShouldSignal<CounterType, Direction>(previousValue);
            const bool shouldSignalAfter = ShouldSignal<CounterType, Direction>(currentValue);

            if (shouldSignalBefore != shouldSignalAfter)
            {
                ifSignalStateChangedProc(shouldSignalAfter);
            }
        }

        return currentValue;
    }

    CounterType Release(CounterType delta = 1, ProcRef<void()> ifSignalledProc = ProcRef<void()>(nullptr))
    {
        CounterType previousValue = value.Decrement(delta, MemoryOrder::ACQUIRE_RELEASE);
        CounterType currentValue = previousValue - delta;

        if (ifSignalledProc.IsValid() && ShouldSignal<CounterType, Direction>(currentValue))
        {
            ifSignalledProc();
        }

        return currentValue;
    }

    CounterType Produce(CounterType delta = 1, ProcRef<void(bool)> ifSignalStateChangedProc = ProcRef<void(bool)>(nullptr))
    {
        CounterType previousValue = value.Increment(delta, MemoryOrder::ACQUIRE_RELEASE);
        CounterType currentValue = previousValue + delta;

        if (ifSignalStateChangedProc.IsValid())
        {
            const bool shouldSignalBefore = ShouldSignal<CounterType, Direction>(previousValue);
            const bool shouldSignalAfter = ShouldSignal<CounterType, Direction>(currentValue);

            if (shouldSignalBefore != shouldSignalAfter)
            {
                ifSignalStateChangedProc(shouldSignalAfter);
            }
        }

        return currentValue;
    }

    CounterType Produce(CounterType delta = 1, ProcRef<void()> ifSignalledProc = ProcRef<void()>(nullptr))
    {
        CounterType previousValue = value.Increment(delta, MemoryOrder::ACQUIRE_RELEASE);
        CounterType currentValue = previousValue + delta;

        if (ifSignalledProc.IsValid() && ShouldSignal<CounterType, Direction>(currentValue))
        {
            ifSignalledProc();
        }

        return currentValue;
    }

    void WaitForValue(T targetValue) const
    {
        if constexpr (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE)
        {
            while (value.Get(MemoryOrder::ACQUIRE) >= targetValue)
            {
                HYP_WAIT_IDLE();
            }
        }
        else if constexpr (Direction == SemaphoreDirection::WAIT_FOR_POSITIVE)
        {
            while (value.Get(MemoryOrder::ACQUIRE) <= targetValue)
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

    void SetValue(CounterType newValue)
    {
        value.Set(newValue, MemoryOrder::RELEASE);
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

    ConditionVarSemaphoreImpl(CounterType initialValue)
        : value(initialValue)
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
            const CounterType currentValue = value.Get(MemoryOrder::ACQUIRE);

            if (ShouldSignal<CounterType, Direction>(currentValue))
            {
                if (value.CompareExchangeWeak(currentValue, currentValue + (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE ? 1 : -1), MemoryOrder::ACQUIRE_RELEASE))
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

    CounterType Release(CounterType delta = 1, ProcRef<void(bool)> ifSignalStateChangedProc = ProcRef<void(bool)>(nullptr))
    {
        std::lock_guard<std::mutex> lock(mutex);

        const CounterType previousValue = value.Decrement(delta, MemoryOrder::ACQUIRE_RELEASE);
        const CounterType newValue = previousValue - delta;

        if (ifSignalStateChangedProc.IsValid())
        {
            const bool shouldSignalBefore = ShouldSignal<CounterType, Direction>(previousValue);
            const bool shouldSignalAfter = ShouldSignal<CounterType, Direction>(newValue);

            if (shouldSignalBefore != shouldSignalAfter)
            {
                ifSignalStateChangedProc(shouldSignalAfter);
            }
        }

        cv.notify_all();

        return newValue;
    }

    CounterType Release(CounterType delta = 1, ProcRef<void()> ifSignalledProc = ProcRef<void()>(nullptr))
    {
        std::lock_guard<std::mutex> lock(mutex);

        const CounterType newValue = value.Decrement(delta, MemoryOrder::ACQUIRE_RELEASE) - delta;

        if (ifSignalledProc.IsValid() && ShouldSignal<CounterType, Direction>(newValue))
        {
            ifSignalledProc();
        }

        cv.notify_all();

        return newValue;
    }

    CounterType Produce(CounterType delta = 1, ProcRef<void(bool)> ifSignalStateChangedProc = ProcRef<void(bool)>(nullptr))
    {
        std::lock_guard<std::mutex> lock(mutex);

        const CounterType previousValue = value.Increment(delta, MemoryOrder::ACQUIRE_RELEASE);
        const CounterType newValue = previousValue + delta;

        if (ifSignalStateChangedProc.IsValid())
        {
            const bool shouldSignalBefore = ShouldSignal<CounterType, Direction>(previousValue);
            const bool shouldSignalAfter = ShouldSignal<CounterType, Direction>(newValue);

            if (shouldSignalBefore != shouldSignalAfter)
            {
                ifSignalStateChangedProc(shouldSignalAfter);
            }
        }

        cv.notify_all();

        return newValue;
    }

    CounterType Produce(CounterType delta = 1, ProcRef<void()> ifSignalledProc = ProcRef<void()>(nullptr))
    {
        std::lock_guard<std::mutex> lock(mutex);

        const CounterType newValue = value.Increment(delta, MemoryOrder::ACQUIRE_RELEASE) + delta;

        if (ifSignalledProc.IsValid() && ShouldSignal<CounterType, Direction>(newValue))
        {
            ifSignalledProc();
        }

        cv.notify_all();

        return newValue;
    }

    void WaitForValue(T targetValue) const
    {
        std::unique_lock<std::mutex> lock(mutex);

        if constexpr (Direction == SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE)
        {
            while (value.Get(MemoryOrder::ACQUIRE) >= targetValue)
            {
                cv.wait(lock);
            }
        }
        else if constexpr (Direction == SemaphoreDirection::WAIT_FOR_POSITIVE)
        {
            while (value.Get(MemoryOrder::ACQUIRE) <= targetValue)
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

    void SetValue(CounterType newValue)
    {
        std::lock_guard<std::mutex> lock(mutex);
        value.Set(newValue, MemoryOrder::RELEASE);
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

    Semaphore(CounterType initialValue = 0)
        : m_impl(initialValue)
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

    HYP_FORCE_INLINE CounterType Release(CounterType delta, ProcRef<void(bool)> ifSignalStateChangedProc)
    {
        return m_impl.Release(delta, ifSignalStateChangedProc);
    }

    HYP_FORCE_INLINE CounterType Release(CounterType delta, ProcRef<void()> ifSignalledProc)
    {
        return m_impl.Release(delta, ifSignalledProc);
    }

    HYP_FORCE_INLINE CounterType Release(CounterType delta = 1)
    {
        return m_impl.Release(delta, ProcRef<void()>(nullptr));
    }

    HYP_FORCE_INLINE CounterType Produce(CounterType increment, ProcRef<void(bool)> ifSignalStateChangedProc)
    {
        return m_impl.Produce(increment, ifSignalStateChangedProc);
    }

    HYP_FORCE_INLINE CounterType Produce(CounterType increment, ProcRef<void()> ifSignalledProc)
    {
        return m_impl.Produce(increment, ifSignalledProc);
    }

    HYP_FORCE_INLINE CounterType Produce(CounterType increment = 1)
    {
        return m_impl.Produce(increment, ProcRef<void()>(nullptr));
    }

    void WaitForValue(CounterType targetValue) const
    {
        m_impl.WaitForValue(targetValue);
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

using AtomicSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;
using ConditionVarSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, ConditionVarSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;

} // namespace threading

using threading::AtomicSemaphore;
using threading::ConditionVarSemaphore;
using threading::Semaphore;
using threading::SemaphoreDirection;

} // namespace Hyperion
