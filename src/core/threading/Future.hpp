/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FUTURE_HPP
#define HYPERION_FUTURE_HPP

#include <core/threading/Semaphore.hpp>
#include <core/threading/Task.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/ValueStorage.hpp>

#include <core/functional/Delegate.hpp>

namespace hyperion {
namespace threading {

template <class T>
struct Future_Impl final
{
    ValueStorage<T> value;
    Semaphore<int32, SemaphoreDirection::WAIT_FOR_POSITIVE> sp;
    Delegate<void> delegate;

    Future_Impl() = default;

    Future_Impl(const Future_Impl& other) = delete;
    Future_Impl& operator=(const Future_Impl& other) = delete;

    Future_Impl(Future_Impl&& other) noexcept = delete;
    Future_Impl& operator=(Future_Impl&& other) noexcept = delete;

    ~Future_Impl()
    {
        if (sp.IsInSignalState())
        {
            value.Destruct();
        }
    }

    template <class Ty>
    void SetValue(Ty&& val)
    {
        AssertDebug(!sp.IsInSignalState());

        value.Construct(std::forward<Ty>(val));

        sp.Produce(1, ProcRef<void()>(&delegate));
    }

    T& GetValue()
    {
        sp.Acquire();

        return value.Get();
    }

    const T& GetValue() const
    {
        sp.Acquire();

        return value.Get();
    }
};

template <class T>
class Future
{
public:
    Future() = default;
    Future(const Future& other) = default;
    Future& operator=(const Future& other) = default;
    Future(Future&& other) noexcept = default;
    Future& operator=(Future&& other) noexcept = default;
    ~Future() = default;

    HYP_FORCE_INLINE T& GetValue() const
    {
        AssertDebugMsg(m_impl != nullptr, "Future is in invalid state!");

        return m_impl->GetValue();
    }

private:
    RC<Future_Impl<T>> m_impl;
};

} // namespace threading

using threading::Future;

} // namespace hyperion

#endif