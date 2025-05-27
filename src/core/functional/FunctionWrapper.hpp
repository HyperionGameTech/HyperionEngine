/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FUNCTION_WRAPPER_HPP
#define HYPERION_FUNCTION_WRAPPER_HPP

#include <core/Traits.hpp>

namespace hyperion {
namespace functional {

template <class T, class T2 = void>
struct FunctionWrapper;

namespace detail {

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

} // namespace detail

template <class T>
struct FunctionWrapper<T, std::enable_if_t<FunctionTraits<T>::is_member_function && !FunctionTraits<T>::is_functor>>
{
    using Type = typename detail::FunctionWrapper_MemberFunctionHelper<T>::Type;

    Type mem_fn;

    // for std::declval usage
    constexpr FunctionWrapper() = default;

    constexpr FunctionWrapper(Type mem_fn)
        : mem_fn(mem_fn)
    {
    }

    constexpr decltype(auto) operator()(typename detail::FunctionWrapper_MemberFunctionHelper<T>::TargetType& value) const
    {
        return (value.*mem_fn)();
    }

    constexpr decltype(auto) operator()(const typename detail::FunctionWrapper_MemberFunctionHelper<T>::TargetType& value) const
    {
        return (value.*mem_fn)();
    }
};

template <class T>
struct FunctionWrapper<T, std::enable_if_t<std::is_member_object_pointer_v<T>>>
{
    using Type = typename detail::FunctionWrapper_MemberHelper<T>::Type;

    Type member;

    // for std::declval usage
    constexpr FunctionWrapper() = default;

    constexpr FunctionWrapper(Type member)
        : member(member)
    {
    }

    constexpr decltype(auto) operator()(typename detail::FunctionWrapper_MemberHelper<T>::TargetType& value) const
    {
        return value.*member;
    }

    constexpr decltype(auto) operator()(const typename detail::FunctionWrapper_MemberHelper<T>::TargetType& value) const
    {
        return value.*member;
    }
};

template <class TargetType, class ReturnType>
struct FunctionWrapper<ReturnType (*)(TargetType)>
{
    using Type = ReturnType (*)(TargetType);

    Type func;

    constexpr FunctionWrapper(Type func)
        : func(func)
    {
    }

    constexpr decltype(auto) operator()(const TargetType& value) const
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

    constexpr decltype(auto) operator()(TargetType& value) const
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

    constexpr decltype(auto) operator()(const TargetType& value) const
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
    constexpr decltype(auto) operator()(Args&&... args) const
    {
        return func(std::forward<Args>(args)...);
    }
};

} // namespace functional

using functional::FunctionWrapper;

} // namespace hyperion

#endif