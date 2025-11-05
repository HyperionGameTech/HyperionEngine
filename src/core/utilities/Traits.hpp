/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

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

template <class T>
struct IsArray : std::false_type
{
};

template <class T>
struct IsFixedArray : std::false_type
{
};

template <class T>
struct IsFlatMap : std::false_type
{
};

template <class T>
struct IsHashMap : std::false_type
{
};

template <class T>
struct IsArrayMap : std::false_type
{
};

template <class T>
struct IsHashSet : std::false_type
{
};

template <class T>
struct IsFlatSet : std::false_type
{
};

template <class T>
struct IsLinkedList : std::false_type
{
};

template <class T>
struct IsVariant : std::false_type
{
};

template <class T>
struct IsString : std::false_type
{
};

} // namespace hyperion
