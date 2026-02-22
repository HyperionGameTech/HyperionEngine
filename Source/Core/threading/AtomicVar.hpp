/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#ifdef HYP_WINDOWS
#include <Windows.h>
#endif

#include <Core/Types.hpp>

#include <atomic>
#include <algorithm>
#include <utility>

namespace Hyperion {

enum class MemoryOrder
{
    RELAXED,
    SEQUENTIAL,
    ACQUIRE,
    RELEASE,
    ACQUIRE_RELEASE
};

namespace threading {
HYP_FORCE_INLINE static constexpr std::memory_order ToCxxMemoryOrder(MemoryOrder order)
{
    switch (order)
    {
    case MemoryOrder::RELAXED:
        return std::memory_order_relaxed;
    case MemoryOrder::SEQUENTIAL:
        return std::memory_order_seq_cst;
    case MemoryOrder::ACQUIRE:
        return std::memory_order_acquire;
    case MemoryOrder::RELEASE:
        return std::memory_order_release;
    case MemoryOrder::ACQUIRE_RELEASE:
        return std::memory_order_acq_rel;
    default:
        return std::memory_order_seq_cst;
    }
}

template <class T, class T2 = void>
class AtomicVar;

template <class T>
class AtomicVar<T, std::enable_if_t<std::is_integral_v<T> || std::is_pointer_v<T>>>
{
    static_assert(
        sizeof(T) <= 8,
        "T must have a sizeof value of <= 8");

    using Type = T;

    std::atomic<Type> m_value;

public:
    AtomicVar()
        : m_value { Type(T {}) }
    {
    }

    AtomicVar(T value)
        : m_value { Type(value) }
    {
    }

    AtomicVar(const AtomicVar& other) = delete;
    AtomicVar& operator=(const AtomicVar& other) = delete;
    AtomicVar(AtomicVar&& other) noexcept = delete;
    AtomicVar& operator=(AtomicVar&& other) noexcept = delete;
    ~AtomicVar() = default;

    HYP_FORCE_INLINE T Get(MemoryOrder order) const
    {
        return m_value.load(ToCxxMemoryOrder(order));
    }

    HYP_FORCE_INLINE void Set(T value, MemoryOrder order)
    {
        m_value.store(value, ToCxxMemoryOrder(order));
    }

    HYP_FORCE_INLINE T Exchange(T newValue, MemoryOrder order)
    {
        return m_value.exchange(newValue, ToCxxMemoryOrder(order));
    }

    HYP_FORCE_INLINE bool CompareExchangeWeak(T& expected, T desired, MemoryOrder order)
    {
        return m_value.compareExchangeWeak(expected, desired, ToCxxMemoryOrder(order));
    }

    HYP_FORCE_INLINE bool CompareExchangeStrong(T& expected, T desired, MemoryOrder order)
    {
        return m_value.compareExchangeStrong(expected, desired, ToCxxMemoryOrder(order));
    }

    HYP_FORCE_INLINE T Increment(T amount, MemoryOrder order)
    {
        return m_value.fetch_add(amount, ToCxxMemoryOrder(order));
    }

    HYP_FORCE_INLINE T Decrement(T amount, MemoryOrder order)
    {
        return m_value.fetch_sub(amount, ToCxxMemoryOrder(order));
    }

    HYP_FORCE_INLINE T BitOr(T value, MemoryOrder order)
    {
        return m_value.fetch_or(value, ToCxxMemoryOrder(order));
    }

    HYP_FORCE_INLINE T BitAnd(T value, MemoryOrder order)
    {
        return m_value.fetch_and(value, ToCxxMemoryOrder(order));
    }

    HYP_FORCE_INLINE T BitXor(T value, MemoryOrder order)
    {
        return m_value.fetch_xor(value, ToCxxMemoryOrder(order));
    }
};

template <class T>
class AtomicVar<T, std::enable_if_t<std::is_enum_v<T>>>
{
    using Type = std::underlying_type_t<T>;

    static_assert(
        sizeof(T) <= 8,
        "T must have a sizeof value of <= 8");

    std::atomic<Type> m_value;

public:
    AtomicVar()
        : m_value { Type(T {}) }
    {
    }

    AtomicVar(T value)
        : m_value { Type(value) }
    {
    }

    AtomicVar(const AtomicVar& other) = delete;
    AtomicVar& operator=(const AtomicVar& other) = delete;
    AtomicVar(AtomicVar&& other) noexcept = delete;
    AtomicVar& operator=(AtomicVar&& other) noexcept = delete;
    ~AtomicVar() = default;

    HYP_FORCE_INLINE T Get(MemoryOrder order) const
    {
        return T(m_value.load(ToCxxMemoryOrder(order)));
    }

    HYP_FORCE_INLINE void Set(T value, MemoryOrder order)
    {
        m_value.store(Type(value), ToCxxMemoryOrder(order));
    }

    HYP_FORCE_INLINE T Exchange(T newValue, MemoryOrder order)
    {
        return T(m_value.exchange(Type(newValue), ToCxxMemoryOrder(order)));
    }
};

#ifdef HYP_WINDOWS

//! Returns the original value before addition
static inline int32 AtomicAdd(volatile int32* value, int32 amount)
{
    return _InterlockedExchangeAdd(reinterpret_cast<long volatile*>(value), amount);
}

//! Returns the original value before addition
static inline int64 AtomicAdd(volatile int64* value, int64 amount)
{
    return _InterlockedExchangeAdd64(reinterpret_cast<long long volatile*>(value), amount);
}

//! Returns the original value before subtraction
static inline int32 AtomicSub(volatile int32* value, int32 amount)
{
    return _InterlockedExchangeAdd(reinterpret_cast<long volatile*>(value), -amount);
}

//! Returns the original value before subtraction
static inline int64 AtomicSub(volatile int64* value, int64 amount)
{
    return _InterlockedExchangeAdd64(reinterpret_cast<long long volatile*>(value), -amount);
}

//! Returns the incremented value
static inline int32 AtomicIncrement(volatile int32* value)
{
    return _InterlockedIncrement(reinterpret_cast<long volatile*>(value));
}

//! Returns the incremented value
static inline int64 AtomicIncrement(volatile int64* value)
{
    return _InterlockedIncrement64(reinterpret_cast<long long volatile*>(value));
}

//! Returns the decremented value
static inline int32 AtomicDecrement(volatile int32* value)
{
    return _InterlockedDecrement(reinterpret_cast<long volatile*>(value));
}

//! Returns the decremented value
static inline int64 AtomicDecrement(volatile int64* value)
{
    return _InterlockedDecrement64(reinterpret_cast<long long volatile*>(value));
}

//! Returns the original value before exchange
static inline int32 AtomicExchange(volatile int32* value, int32 newValue)
{
    return _InterlockedExchange(reinterpret_cast<long volatile*>(value), newValue);
}

//! Returns the original value before exchange
static inline int64 AtomicExchange(volatile int64* value, int64 newValue)
{
    return _InterlockedExchange64(reinterpret_cast<long long volatile*>(value), newValue);
}

//! Returns true if the exchange was successful, false if the expected value did not match
/// Modifies the expected value to the current value of the variable
static inline bool AtomicCompareExchange(volatile int32* value, int32& expected, int32 desired)
{
    int32 oldValue = _InterlockedCompareExchange(reinterpret_cast<long volatile*>(value), desired, expected);
    if (oldValue == expected)
    {
        expected = oldValue;
        return true;
    }
    else
    {
        expected = oldValue;
        return false;
    }
}

//! Returns true if the exchange was successful, false if the expected value did not match
static inline bool AtomicCompareExchange(volatile int64* value, int64& expected, int64 desired)
{
    int64 oldValue = _InterlockedCompareExchange64(reinterpret_cast<long long volatile*>(value), desired, expected);
    if (oldValue == expected)
    {
        expected = oldValue;
        return true;
    }
    else
    {
        expected = oldValue;
        return false;
    }
}

//! Returns the original value before bitwise OR
static inline int32 AtomicBitOr(volatile int32* value, int32 bitMask)
{
    return _InterlockedOr(reinterpret_cast<long volatile*>(value), bitMask);
}

//! Returns the original value before bitwise OR
static inline int64 AtomicBitOr(volatile int64* value, int64 bitMask)
{
    return _InterlockedOr64(reinterpret_cast<long long volatile*>(value), bitMask);
}

//! Returns the original value before bitwise AND
static inline int32 AtomicBitAnd(volatile int32* value, int32 bitMask)
{
    return _InterlockedAnd(reinterpret_cast<long volatile*>(value), bitMask);
}

//! Returns the original value before bitwise AND
static inline int64 AtomicBitAnd(volatile int64* value, int64 bitMask)
{
    return _InterlockedAnd64(reinterpret_cast<long long volatile*>(value), bitMask);
}

//! Returns the original value before bitwise XOR
static inline int32 AtomicBitXor(volatile int32* value, int32 bitMask)
{
    return _InterlockedXor(reinterpret_cast<long volatile*>(value), bitMask);
}

//! Returns the original value before bitwise XOR
static inline int64 AtomicBitXor(volatile int64* value, int64 bitMask)
{
    return _InterlockedXor64(reinterpret_cast<long long volatile*>(value), bitMask);
}

#else

//! Returns the original value before addition
static inline int32 AtomicAdd(volatile int32* value, int32 amount)
{
    return __atomic_fetch_add(value, amount, __ATOMIC_SEQ_CST);
}

//! Returns the original value before addition
static inline int64 AtomicAdd(volatile int64* value, int64 amount)
{
    return __atomic_fetch_add(value, amount, __ATOMIC_SEQ_CST);
}

//! Returns the original value before subtraction
static inline int32 AtomicSub(volatile int32* value, int32 amount)
{
    return __atomic_fetch_sub(value, amount, __ATOMIC_SEQ_CST);
}

//! Returns the original value before subtraction
static inline int64 AtomicSub(volatile int64* value, int64 amount)
{
    return __atomic_fetch_sub(value, amount, __ATOMIC_SEQ_CST);
}

//! Returns the incremented value
static inline int32 AtomicIncrement(volatile int32* value)
{
    return __atomic_fetch_add(value, 1, __ATOMIC_SEQ_CST) + 1;
}

//! Returns the incremented value
static inline int64 AtomicIncrement(volatile int64* value)
{
    return __atomic_fetch_add(value, 1, __ATOMIC_SEQ_CST) + 1;
}

//! Returns the decremented value
static inline int32 AtomicDecrement(volatile int32* value)
{
    return __atomic_fetch_sub(value, 1, __ATOMIC_SEQ_CST) - 1;
}

//! Returns the decremented value
static inline int64 AtomicDecrement(volatile int64* value)
{
    return __atomic_fetch_sub(value, 1, __ATOMIC_SEQ_CST) - 1;
}

//! Returns the original value before exchange
static inline int32 AtomicExchange(volatile int32* value, int32 newValue)
{
    return __atomic_exchange_n(value, newValue, __ATOMIC_SEQ_CST);
}

//! Returns the original value before exchange
static inline int64 AtomicExchange(volatile int64* value, int64 newValue)
{
    return __atomic_exchange_n(value, newValue, __ATOMIC_SEQ_CST);
}

//! Returns true if the exchange was successful, false if the expected value did not match
static inline bool AtomicCompareExchange(volatile int32* value, int32& expected, int32 desired)
{
    return __atomic_compare_exchange_n(value, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

//! Returns true if the exchange was successful, false if the expected value did not match
static inline bool AtomicCompareExchange(volatile int64* value, int64& expected, int64 desired)
{
    return __atomic_compare_exchange_n(value, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

//! Returns the original value before bitwise OR
static inline int32 AtomicBitOr(volatile int32* value, int32 bitMask)
{
    return __atomic_fetch_or(value, bitMask, __ATOMIC_SEQ_CST);
}

//! Returns the original value before bitwise OR
static inline int64 AtomicBitOr(volatile int64* value, int64 bitMask)
{
    return __atomic_fetch_or(value, bitMask, __ATOMIC_SEQ_CST);
}

//! Returns the original value before bitwise AND
static inline int32 AtomicBitAnd(volatile int32* value, int32 bitMask)
{
    return __atomic_fetch_and(value, bitMask, __ATOMIC_SEQ_CST);
}

//! Returns the original value before bitwise AND
static inline int64 AtomicBitAnd(volatile int64* value, int64 bitMask)
{
    return __atomic_fetch_and(value, bitMask, __ATOMIC_SEQ_CST);
}

//! Returns the original value before bitwise XOR
static inline int32 AtomicBitXor(volatile int32* value, int32 bitMask)
{
    return __atomic_fetch_xor(value, bitMask, __ATOMIC_SEQ_CST);
}

//! Returns the original value before bitwise XOR
static inline int64 AtomicBitXor(volatile int64* value, int64 bitMask)
{
    return __atomic_fetch_xor(value, bitMask, __ATOMIC_SEQ_CST);
}

#endif

} // namespace threading

using threading::AtomicAdd;
using threading::AtomicBitAnd;
using threading::AtomicBitOr;
using threading::AtomicBitXor;
using threading::AtomicCompareExchange;
using threading::AtomicDecrement;
using threading::AtomicExchange;
using threading::AtomicIncrement;
using threading::AtomicSub;
using threading::AtomicVar;

} // namespace Hyperion
