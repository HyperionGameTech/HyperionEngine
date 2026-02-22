/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/utilities/FunctionTraits.hpp>

namespace Hyperion {
namespace functional {

template <class T, class T2 = void>
struct FunctionWrapper;

template <class T>
struct FunctionWrapper_MemberFunctionHelper;

template <class TTargetType, class TReturnType>
struct FunctionWrapper_MemberFunctionHelper<TReturnType (TTargetType::*)()>
{
    using TargetType = TTargetType;
    using ReturnType = TReturnType;

    using Type = TReturnType (TTargetType::*)();
};

template <class TTargetType, class TReturnType>
struct FunctionWrapper_MemberFunctionHelper<TReturnType (TTargetType::*)() const>
{
    using TargetType = TTargetType;
    using ReturnType = TReturnType;

    using Type = TReturnType (TTargetType::*)() const;
};

template <class T>
struct FunctionWrapper_MemberHelper;

template <class TTargetType, class TReturnType>
struct FunctionWrapper_MemberHelper<TReturnType TTargetType::*>
{
    using TargetType = TTargetType;
    using ReturnType = TReturnType;

    using Type = TReturnType TTargetType::*;
};

template <class T>
struct FunctionWrapper<T, std::enable_if_t<FunctionTraits<T>::isNonconstMemberFunction && !FunctionTraits<T>::isFunctor>>
{
    using Type = typename FunctionWrapper_MemberFunctionHelper<T>::Type;

    Type memFn;

    // for std::declval usage
    constexpr FunctionWrapper() = default;

    constexpr FunctionWrapper(Type memFn)
        : memFn(memFn)
    {
    }

    HYP_FORCE_INLINE constexpr decltype(auto) operator()(typename FunctionWrapper_MemberFunctionHelper<T>::TargetType* value) const
    {
        if (!value)
        {
            return typename FunctionWrapper_MemberFunctionHelper<T>::ReturnType();
        }

        return (value->*memFn)();
    }

    HYP_FORCE_INLINE constexpr decltype(auto) operator()(typename FunctionWrapper_MemberFunctionHelper<T>::TargetType& value) const
    {
        return (value.*memFn)();
    }

    HYP_FORCE_INLINE constexpr decltype(auto) operator()(const typename FunctionWrapper_MemberFunctionHelper<T>::TargetType* value) const
    {
        if (!value)
        {
            return typename FunctionWrapper_MemberFunctionHelper<T>::ReturnType();
        }

        return (value->*memFn)();
    }

    HYP_FORCE_INLINE constexpr decltype(auto) operator()(const typename FunctionWrapper_MemberFunctionHelper<T>::TargetType& value) const
    {
        return (value.*memFn)();
    }
};

template <class T>
struct FunctionWrapper<T, std::enable_if_t<FunctionTraits<T>::isConstMemberFunction && !FunctionTraits<T>::isFunctor>>
{
    using Type = typename FunctionWrapper_MemberFunctionHelper<T>::Type;

    Type memFn;

    // for std::declval usage
    constexpr FunctionWrapper() = default;

    constexpr FunctionWrapper(Type memFn)
        : memFn(memFn)
    {
    }

    HYP_FORCE_INLINE constexpr decltype(auto) operator()(const typename FunctionWrapper_MemberFunctionHelper<T>::TargetType* value) const
    {
        if (!value)
        {
            return typename FunctionWrapper_MemberFunctionHelper<T>::ReturnType();
        }

        return (value->*memFn)();
    }

    HYP_FORCE_INLINE constexpr decltype(auto) operator()(const typename FunctionWrapper_MemberFunctionHelper<T>::TargetType& value) const
    {
        return (value.*memFn)();
    }
};

template <class T>
struct FunctionWrapper<T, std::enable_if_t<std::is_member_object_pointer_v<T>>>
{
    using Type = typename FunctionWrapper_MemberHelper<T>::Type;

    Type member;

    // for std::declval usage
    constexpr FunctionWrapper() = default;

    constexpr FunctionWrapper(Type member)
        : member(member)
    {
    }

    HYP_FORCE_INLINE constexpr decltype(auto) operator()(typename FunctionWrapper_MemberHelper<T>::TargetType* value) const
    {
        if (!value)
        {
            return typename FunctionWrapper_MemberHelper<T>::ReturnType();
        }

        return value->*member;
    }

    HYP_FORCE_INLINE constexpr decltype(auto) operator()(typename FunctionWrapper_MemberHelper<T>::TargetType& value) const
    {
        return value.*member;
    }

    HYP_FORCE_INLINE constexpr decltype(auto) operator()(const typename FunctionWrapper_MemberHelper<T>::TargetType* value) const
    {
        if (!value)
        {
            return typename FunctionWrapper_MemberHelper<T>::ReturnType();
        }

        return value->*member;
    }

    HYP_FORCE_INLINE constexpr decltype(auto) operator()(const typename FunctionWrapper_MemberHelper<T>::TargetType& value) const
    {
        return value.*member;
    }
};

template <class TargetType, class ReturnType>
struct FunctionWrapper<ReturnType (*)(TargetType)>
{
    using Type = ReturnType (*)(TargetType);

    Type func;

    HYP_FORCE_INLINE constexpr FunctionWrapper(Type func)
        : func(func)
    {
    }

    HYP_FORCE_INLINE constexpr decltype(auto) operator()(const TargetType& value) const
    {
        return func(value);
    }
};

template <class TargetType, class ReturnType>
struct FunctionWrapper<ReturnType (*)(TargetType&)>
{
    using Type = ReturnType (*)(TargetType&);

    Type func;

    // for std::declval usage
    constexpr FunctionWrapper() = default;

    constexpr FunctionWrapper(Type func)
        : func(func)
    {
    }

    HYP_FORCE_INLINE constexpr decltype(auto) operator()(TargetType& value) const
    {
        return func(value);
    }
};

template <class TargetType, class ReturnType>
struct FunctionWrapper<ReturnType (*)(const TargetType&)>
{
    using Type = ReturnType (*)(const TargetType&);

    Type func;

    // for std::declval usage
    constexpr FunctionWrapper() = default;

    constexpr FunctionWrapper(Type func)
        : func(func)
    {
    }

    HYP_FORCE_INLINE constexpr decltype(auto) operator()(const TargetType& value) const
    {
        return func(value);
    }
};

// General lambda / functor
template <class T, class T2>
struct FunctionWrapper
{
    T func;

    // for std::declval usage
    constexpr FunctionWrapper() = default;

    template <class Func>
    constexpr FunctionWrapper(Func&& func)
        : func(std::forward<Func>(func))
    {
    }

    template <class... Args>
    HYP_FORCE_INLINE constexpr decltype(auto) operator()(Args&&... args) const
    {
        return func(std::forward<Args>(args)...);
    }
};

} // namespace functional

using functional::FunctionWrapper;

} // namespace Hyperion
