/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TRAITS_HPP
#define HYPERION_TRAITS_HPP

#include <core/utilities/Tuple.hpp>

#include <type_traits>

namespace hyperion {

template <bool DefaultConstructible, bool Copyable, bool Moveable, class Type>
struct ConstructAssignmentTraits { };

template <class Type>
struct ConstructAssignmentTraits<true, false, false, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = delete;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = delete;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = delete;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = delete;
};

template <class Type>
struct ConstructAssignmentTraits<true, true, false, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = default;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = default;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = delete;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = delete;
};

template <class Type>
struct ConstructAssignmentTraits<true, true, true, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = default;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = default;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = default;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = default;
};

template <class Type>
struct ConstructAssignmentTraits<true, false, true, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = delete;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = delete;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = default;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = default;
};


template <class Type>
struct ConstructAssignmentTraits<false, false, false, Type>
{
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = delete;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = delete;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = delete;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = delete;

protected:
    constexpr ConstructAssignmentTraits() noexcept = default;
};

template <class Type>
struct ConstructAssignmentTraits<false, true, false, Type>
{
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = default;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = default;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = delete;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = delete;

protected:
    constexpr ConstructAssignmentTraits() noexcept = default;
};

template <class Type>
struct ConstructAssignmentTraits<false, true, true, Type>
{
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = default;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = default;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = default;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = default;

protected:
    constexpr ConstructAssignmentTraits() noexcept = default;
};

template <class Type>
struct ConstructAssignmentTraits<false, false, true, Type>
{
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits &other) noexcept = delete;
    ConstructAssignmentTraits &operator=(const ConstructAssignmentTraits &other) noexcept = delete;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits &&other) noexcept = default;
    ConstructAssignmentTraits &operator=(ConstructAssignmentTraits &&other) noexcept = default;

protected:
    constexpr ConstructAssignmentTraits() noexcept = default;
};

#pragma region IsFunctor

template <class T, class T2 = void>
struct IsFunctor
{
    static constexpr bool value = false;
};

template <class T>
struct IsFunctor<T, std::enable_if_t< implementation_exists< decltype(&T::operator()) > > >
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
    using ReturnType = R;
    using ArgTypes = Tuple<Args...>;
    using ThisType = void;

    static constexpr uint num_args = sizeof...(Args);
    static constexpr bool is_member_function = false;
    static constexpr bool is_functor = false;
    static constexpr bool is_function_pointer = false;

    template <uint N>
    struct Arg
    {
        static_assert(N < num_args, "Invalid argument index");
        using Type = typename std::tuple_element<N, std::tuple<Args...>>::type;
    };
};

template <class R, class... Args>
struct FunctionTraits<R(*)(Args...)> : public FunctionTraits<R(Args...)>
{
    static constexpr bool is_function_pointer = true;
};

template <class R, class C, class... Args>
struct FunctionTraits<R(C::*)(Args...), std::enable_if_t< !IsFunctor<R(C::*)(Args...) >::value > > : public FunctionTraits<R(Args...)>
{
    using ThisType = C;

    static constexpr bool is_member_function = true;
};

template <class R, class C, class... Args>
struct FunctionTraits<R(C::*)(Args...) const, std::enable_if_t< !IsFunctor<R(C::*)(Args...) const>::value > > : public FunctionTraits<R(Args...)>
{
    using ThisType = C;

    static constexpr bool is_member_function = true;
};

template <class R, class C, class... Args>
struct FunctionTraits<R(C::*)(Args...) volatile, std::enable_if_t< !IsFunctor<R(C::*)(Args...) volatile>::value > > : public FunctionTraits<R(Args...)>
{
    using ThisType = C;

    static constexpr bool is_member_function = true;
};

template <class R, class C, class... Args>
struct FunctionTraits<R(C::*)(Args...) const volatile, std::enable_if_t< !IsFunctor<R(C::*)(Args...) const volatile>::value > > : public FunctionTraits<R(Args...) >
{
    using ThisType = C;

    static constexpr bool is_member_function = true;
};

template <class T>
struct FunctionTraits<T, std::enable_if_t< IsFunctor<T>::value > > : public FunctionTraits< decltype(&T::operator()) >
{
    static constexpr bool is_functor = true;
};


// template <class R, class C, class... Args>
// struct FunctionTraits<R(C::*)(Args...), std::enable_if_t< IsFunctor<R(C::*)(Args...) >::value > > : public FunctionTraits<R(Args...)>
// {
//     using ThisType = C;

//     static constexpr bool is_member_function = true;
// };

// template <class R, class C, class... Args>
// struct FunctionTraits<R(C::*)(Args...) const, std::enable_if_t< IsFunctor<R(C::*)(Args...) const>::value > > : public FunctionTraits<R(Args...)>
// {
//     using ThisType = C;

//     static constexpr bool is_member_function = true;
// };

// template <class R, class C, class... Args>
// struct FunctionTraits<R(C::*)(Args...) volatile, std::enable_if_t< IsFunctor<R(C::*)(Args...) volatile>::value > > : public FunctionTraits<R(Args...)>
// {
//     using ThisType = C;

//     static constexpr bool is_member_function = true;
// };

// template <class R, class C, class... Args>
// struct FunctionTraits<R(C::*)(Args...) const volatile, std::enable_if_t< IsFunctor<R(C::*)(Args...) const volatile>::value > > : public FunctionTraits<R(Args...) >
// {
//     using ThisType = C;

//     static constexpr bool is_member_function = true;
// };


template <class T>
struct FunctionTraits<T &> : public FunctionTraits<T> {};

template <class T>
struct FunctionTraits<T &&> : public FunctionTraits<T> {};

template <class T>
struct FunctionTraits<T *> : public FunctionTraits<T> {};

template <class T>
struct FunctionTraits<T const> : public FunctionTraits<T> {};

template <class T>
struct FunctionTraits<T volatile> : public FunctionTraits<T> {};

template <class T>
struct FunctionTraits<T const volatile> : public FunctionTraits<T> {};

#pragma endregion FunctionTraits

} // namespace hyperion

#endif