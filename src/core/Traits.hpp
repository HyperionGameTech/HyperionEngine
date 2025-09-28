/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/Tuple.hpp>

#include <type_traits>

namespace hyperion {

template <bool DefaultConstructible, bool Copyable, bool Moveable, class Type>
struct ConstructAssignmentTraits
{
};

template <class Type>
struct ConstructAssignmentTraits<true, false, false, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits& other) noexcept = delete;
    ConstructAssignmentTraits& operator=(const ConstructAssignmentTraits& other) noexcept = delete;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits&& other) noexcept = delete;
    ConstructAssignmentTraits& operator=(ConstructAssignmentTraits&& other) noexcept = delete;
};

template <class Type>
struct ConstructAssignmentTraits<true, true, false, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits& other) noexcept = default;
    ConstructAssignmentTraits& operator=(const ConstructAssignmentTraits& other) noexcept = default;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits&& other) noexcept = delete;
    ConstructAssignmentTraits& operator=(ConstructAssignmentTraits&& other) noexcept = delete;
};

template <class Type>
struct ConstructAssignmentTraits<true, true, true, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits& other) noexcept = default;
    ConstructAssignmentTraits& operator=(const ConstructAssignmentTraits& other) noexcept = default;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits&& other) noexcept = default;
    ConstructAssignmentTraits& operator=(ConstructAssignmentTraits&& other) noexcept = default;
};

template <class Type>
struct ConstructAssignmentTraits<true, false, true, Type>
{
    constexpr ConstructAssignmentTraits() noexcept = default;
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits& other) noexcept = delete;
    ConstructAssignmentTraits& operator=(const ConstructAssignmentTraits& other) noexcept = delete;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits&& other) noexcept = default;
    ConstructAssignmentTraits& operator=(ConstructAssignmentTraits&& other) noexcept = default;
};

template <class Type>
struct ConstructAssignmentTraits<false, false, false, Type>
{
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits& other) noexcept = delete;
    ConstructAssignmentTraits& operator=(const ConstructAssignmentTraits& other) noexcept = delete;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits&& other) noexcept = delete;
    ConstructAssignmentTraits& operator=(ConstructAssignmentTraits&& other) noexcept = delete;

protected:
    constexpr ConstructAssignmentTraits() noexcept = default;
};

template <class Type>
struct ConstructAssignmentTraits<false, true, false, Type>
{
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits& other) noexcept = default;
    ConstructAssignmentTraits& operator=(const ConstructAssignmentTraits& other) noexcept = default;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits&& other) noexcept = delete;
    ConstructAssignmentTraits& operator=(ConstructAssignmentTraits&& other) noexcept = delete;

protected:
    constexpr ConstructAssignmentTraits() noexcept = default;
};

template <class Type>
struct ConstructAssignmentTraits<false, true, true, Type>
{
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits& other) noexcept = default;
    ConstructAssignmentTraits& operator=(const ConstructAssignmentTraits& other) noexcept = default;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits&& other) noexcept = default;
    ConstructAssignmentTraits& operator=(ConstructAssignmentTraits&& other) noexcept = default;

protected:
    constexpr ConstructAssignmentTraits() noexcept = default;
};

template <class Type>
struct ConstructAssignmentTraits<false, false, true, Type>
{
    constexpr ConstructAssignmentTraits(const ConstructAssignmentTraits& other) noexcept = delete;
    ConstructAssignmentTraits& operator=(const ConstructAssignmentTraits& other) noexcept = delete;
    constexpr ConstructAssignmentTraits(ConstructAssignmentTraits&& other) noexcept = default;
    ConstructAssignmentTraits& operator=(ConstructAssignmentTraits&& other) noexcept = default;

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
    using ArgTypes = Tuple<Args...>;
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
        using Type = typename std::tuple_element<N, std::tuple<Args...>>::type;
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

/*! \brief This macro generates a struct that has a static constexpr bool value that indicates if the type T has a member function with the name methodName.
 *  \param methodName The name of the method to check for.
 *  \details Usage:
 *      HYP_MAKE_HAS_METHOD(ToString);
 *
 *      static_assert(HYP_HAS_METHOD(MyType, ToString), "MyType must have a ToString method");
 */
#define HYP_MAKE_HAS_METHOD(methodName)                                                                             \
    template <class T, class Enabled = void>                                                                        \
    struct HasMethod_##methodName                                                                                   \
    {                                                                                                               \
        static constexpr bool value = false;                                                                        \
    };                                                                                                              \
                                                                                                                    \
    template <class T>                                                                                              \
    struct HasMethod_##methodName<T, std::enable_if_t<std::is_member_function_pointer_v<decltype(&T::methodName)>>> \
    {                                                                                                               \
        static constexpr bool value = true;                                                                         \
    }

#define HYP_HAS_METHOD(T, methodName) HasMethod_##methodName<T>::value

/*! \brief This macro generates a struct that has a static constexpr bool value that indicates if the type T has a static member function with the name methodName.
 *  \param methodName The name of the method to check for.
 *  \details Usage:
 *      HYP_MAKE_HAS_STATIC_METHOD(ToString);
 *
 *      static_assert(HYP_HAS_STATIC_METHOD(MyType, Create), "MyType must have a Create static method");
 */
#define HYP_MAKE_HAS_STATIC_METHOD(methodName)                                                             \
    template <class T, class Enabled = void>                                                               \
    struct HasStaticMethod_##methodName                                                                    \
    {                                                                                                      \
        static constexpr bool value = false;                                                               \
    };                                                                                                     \
                                                                                                           \
    template <class T>                                                                                     \
    struct HasStaticMethod_##methodName<T, std::enable_if_t<std::is_function_v<decltype(&T::methodName)>>> \
    {                                                                                                      \
        static constexpr bool value = true;                                                                \
    }

#define HYP_HAS_STATIC_METHOD(T, methodName) HasStaticMethod_##methodName<T>::value

} // namespace hyperion
