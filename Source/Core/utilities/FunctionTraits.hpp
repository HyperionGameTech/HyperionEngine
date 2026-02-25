/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Types.hpp>
#include <Core/Constants.hpp>

#include <type_traits>

namespace Hyperion {

// lghtweight compile-time type tuple for FunctionTraits to avoid dependency on Tuple
template <class... Ts>
struct ArgTypeList
{
    static constexpr uint32 size = uint32(sizeof...(Ts));
};

namespace detail {

// Primary indexer over a parameter pack
template <uint32 N, class... Ts>
struct NthArgType;

template <class Head, class... Tail>
struct NthArgType<0, Head, Tail...>
{
    using Type = Head;
};

template <uint32 N, class Head, class... Tail>
struct NthArgType<N, Head, Tail...> : NthArgType<N - 1, Tail...>
{
};

// Adapter to index into a ArgTypeList
template <uint32 N, class Tuple>
struct NthArgType_FromTuple;

template <uint32 N, class... Ts>
struct NthArgType_FromTuple<N, ArgTypeList<Ts...>>
{
    using Type = typename NthArgType<N, Ts...>::Type;
};

} // namespace detail

#pragma region IsFunctor

template <class T, class T2 = void>
struct IsFunctor
{
    static constexpr bool value = false;
};

template <class T>
struct IsFunctor<T, std::enable_if_t<ImplementationExistsV<decltype(&T::operator())>>>
{
    static constexpr bool value = true;
};

#pragma endregion IsFunctor

#pragma region FunctionTraits

template <class T, class T2 = void>
struct FunctionTraits;

template <class R, class... Args>
struct FunctionTraits<R(Args...)>
{
    using Type = R(Args...);

    using ReturnType = R;
    using ArgTypes = ArgTypeList<Args...>;
    using ThisType = void;

    static constexpr uint32 numArgs = sizeof...(Args);
    static constexpr bool isMemberFunction = false;
    static constexpr bool isNonconstMemberFunction = false;
    static constexpr bool isConstMemberFunction = false;
    static constexpr bool isVolatileMemberFunction = false;
    static constexpr bool isFunctor = false;
    static constexpr bool isFunctionPointer = false;

    template <uint32 N>
    struct Arg
    {
        static_assert(N < numArgs, "Invalid argument index");
        using Type = typename detail::NthArgType_FromTuple<N, ArgTypes>::Type;
    };
};

template <class R, class... Args>
struct FunctionTraits<R (*)(Args...)> : public FunctionTraits<R(Args...)>
{
    using Type = R (*)(Args...);

    static constexpr bool isFunctionPointer = true;
};

template <class R, class C, class... Args>
struct FunctionTraits<R (C::*)(Args...), std::enable_if_t<!IsFunctor<R (C::*)(Args...)>::value>> : public FunctionTraits<R(Args...)>
{
    using Type = R (C::*)(Args...);

    using ThisType = C;

    static constexpr bool isMemberFunction = true;
    static constexpr bool isNonconstMemberFunction = true;
};

template <class R, class C, class... Args>
struct FunctionTraits<R (C::*)(Args...) const, std::enable_if_t<!IsFunctor<R (C::*)(Args...) const>::value>> : public FunctionTraits<R(Args...)>
{
    using Type = R (C::*)(Args...) const;

    using ThisType = C;

    static constexpr bool isMemberFunction = true;
    static constexpr bool isConstMemberFunction = true;
};

template <class R, class C, class... Args>
struct FunctionTraits<R (C::*)(Args...) volatile, std::enable_if_t<!IsFunctor<R (C::*)(Args...) volatile>::value>> : public FunctionTraits<R(Args...)>
{
    using Type = R (C::*)(Args...) volatile;

    using ThisType = C;

    static constexpr bool isMemberFunction = true;
    static constexpr bool isNonconstMemberFunction = true;
    static constexpr bool isVolatileMemberFunction = true;
};

template <class R, class C, class... Args>
struct FunctionTraits<R (C::*)(Args...) const volatile, std::enable_if_t<!IsFunctor<R (C::*)(Args...) const volatile>::value>> : public FunctionTraits<R(Args...)>
{
    using Type = R (C::*)(Args...) const volatile;

    using ThisType = C;

    static constexpr bool isMemberFunction = true;
    static constexpr bool isConstMemberFunction = true;
    static constexpr bool isVolatileMemberFunction = true;
};

template <class T>
struct FunctionTraits<T, std::enable_if_t<IsFunctor<T>::value>> : public FunctionTraits<decltype(&T::operator())>
{
    using Type = T;

    static constexpr bool isFunctor = true;
};

// template <class R, class C, class... Args>
// struct FunctionTraits<R(C::*)(Args...), std::enable_if_t< IsFunctor<R(C::*)(Args...) >::value > > : public FunctionTraits<R(Args...)>
// {
//     using ThisType = C;

//     static constexpr bool isMemberFunction = true;
// };

// template <class R, class C, class... Args>
// struct FunctionTraits<R(C::*)(Args...) const, std::enable_if_t< IsFunctor<R(C::*)(Args...) const>::value > > : public FunctionTraits<R(Args...)>
// {
//     using ThisType = C;

//     static constexpr bool isMemberFunction = true;
// };

// template <class R, class C, class... Args>
// struct FunctionTraits<R(C::*)(Args...) volatile, std::enable_if_t< IsFunctor<R(C::*)(Args...) volatile>::value > > : public FunctionTraits<R(Args...)>
// {
//     using ThisType = C;

//     static constexpr bool isMemberFunction = true;
// };

// template <class R, class C, class... Args>
// struct FunctionTraits<R(C::*)(Args...) const volatile, std::enable_if_t< IsFunctor<R(C::*)(Args...) const volatile>::value > > : public FunctionTraits<R(Args...) >
// {
//     using ThisType = C;

//     static constexpr bool isMemberFunction = true;
// };

template <class T>
struct FunctionTraits<T&> : public FunctionTraits<T>
{
    using Type = T&;
};

template <class T>
struct FunctionTraits<T&&> : public FunctionTraits<T>
{
};

template <class T>
struct FunctionTraits<T*> : public FunctionTraits<T>
{
    using Type = T*;
};

template <class T>
struct FunctionTraits<T const> : public FunctionTraits<T>
{
    using Type = T const;
};

template <class T>
struct FunctionTraits<T volatile> : public FunctionTraits<T>
{
    using Type = T volatile;
};

template <class T>
struct FunctionTraits<T const volatile> : public FunctionTraits<T>
{
    using Type = T const volatile;
};

#pragma endregion FunctionTraits

} // namespace Hyperion
