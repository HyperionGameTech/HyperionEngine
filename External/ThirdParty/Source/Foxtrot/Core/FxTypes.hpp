#pragma once

#include <cstdint>

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

using float32 = float;
using float64 = double;


constexpr uint64 FxUnitByte = 1;
constexpr uint64 FxUnitKibibyte = 1024;
constexpr uint64 FxUnitMebibyte = FxUnitKibibyte * 1024;
constexpr uint64 FxUnitGibibyte = FxUnitMebibyte * 1024;

using FxHandle = uint32;

static constexpr FxHandle FxNullHandle = UINT32_MAX;

#include "FxDefines.hpp"

#include <cstddef>
#include <type_traits>

template <typename T, typename... TTypes>
concept C_IsAnyOf = (std::is_same_v<T, TTypes> || ...);


template <typename T, typename... TTypes>
concept C_IsAnyBaseOf = (std::is_base_of_v<TTypes, T> || ...);

template <typename TPossiblyConst, typename TNonConst>
concept C_IsSameOrConst = std::is_same_v<typename std::remove_const<TPossiblyConst>::type, TNonConst>;

// template <typename ValueType>
// class Optional {
// public:
//     Optional(ValueType &value)
//         : mHasValue(true),
//           mData()
//     {
//     }

//     Optional(std::nullptr_t)
//         : mHasValue(false),
//           mData()
//     {
//     }

//     bool HasValue() const { return mHasValue; }

//     void Clear()
//     {
//         mHasValue = false;
//     }

//     ValueType &operator *() const
//     {
//         if (!HasValue()) {
//             FxPanic("Optional", "Could not retrieve undefined value", 0);
//         }

//         return *reinterpret_cast<ValueType *>(&mData);
//     }

//     ValueType &Get() const { return *this; }

// private:
//     bool mHasValue = false;
//     std::aligned_storage<sizeof(ValueType), std::alignment_of<ValueType>::value> mData;
// };


// #define FX_SPIN_THREAD_GUARD_DEBUG_USE_MUTEX 1

#ifndef FX_SPIN_THREAD_GUARD_DEBUG_USE_MUTEX
#include <atomic>
using FxAtomicFlag = std::atomic_flag;
#else
#include <mutex>
using FxAtomicFlag = std::mutex;
#endif

struct FxSpinThreadGuard
{
public:
    FxSpinThreadGuard(FxAtomicFlag* busy_flag) noexcept
    {
#ifdef FX_SPIN_THREAD_GUARD_DEBUG_USE_MUTEX
        mMutex = busy_flag;

        mMutex->lock();
#else
        mFlag = busy_flag;

        // If the busy flag is set currently, wait until it is cleared
        while (mFlag->test()) {
            mFlag->wait(true);
        }

        // Mark as busy
        mFlag->test_and_set();
        mFlag->notify_one();
#endif
    }

    void Unlock() noexcept {}

    ~FxSpinThreadGuard() noexcept
    {
#ifdef FX_SPIN_THREAD_GUARD_DEBUG_USE_MUTEX
        mMutex->unlock();
#else
        mFlag->clear(std::memory_order_release);
        mFlag->notify_one();
#endif
    }

private:
#ifdef FX_SPIN_THREAD_GUARD_DEBUG_USE_MUTEX
    std::mutex* mMutex;
#else
    FxAtomicFlag* mFlag = nullptr;
#endif
};


// template <typename TResultType>
// struct FxOptional
// {
//     using Type = TResultType;

//     std::aligned_storage_t<sizeof(Type), alignof(Type)> Result;
//     bool bExists;

// public:
//     FxOptional() : bExists(false) {}
//     FxOptional(const TResultType& result) : bExists(true) { ::new ((void*)&result) TResultType(result); }
// };
