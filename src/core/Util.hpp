/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UTIL_HPP
#define HYPERION_UTIL_HPP

#include <core/utilities/ValueStorage.hpp>
#include <core/containers/StaticString.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

namespace detail {

// strip "class " or "struct " from beginning StaticString
template <auto Str>
constexpr auto StripClassOrStruct()
{
    constexpr auto class_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("class ") > >();
    constexpr auto struct_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("struct ") > >();

    if constexpr (class_index != -1 && (struct_index == -1 || class_index <= struct_index)) {
        return Str.template Substr<class_index + 6, Str.Size()>(); // 6 = length of "class "
    } else if constexpr (struct_index != -1 && (class_index == -1 || struct_index <= class_index)) {
        return Str.template Substr<struct_index + 7, Str.Size()>(); // 7 = length of "struct "
    } else {
        return Str;
    }
}

template <auto Str>
constexpr auto StripNestedClassOrStruct()
{
    constexpr auto left_arrow_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("<") > >();
    constexpr auto right_arrow_index = Str.template FindLast< containers::detail::IntegerSequenceFromString< StaticString(">") > >();
    
    if constexpr (left_arrow_index != SizeType(-1) && right_arrow_index != SizeType(-1)) {
        constexpr auto before_left_arrow = StripNestedClassOrStruct< Str.template Substr<0, left_arrow_index>() >();
        constexpr auto inner_left_arrow = StripNestedClassOrStruct< Str.template Substr<left_arrow_index + 1, right_arrow_index>() >();
        // constexpr auto after_right_arrow = Str.template Substr<right_arrow_index + 1, Str.Size() - 1>();
        return ((before_left_arrow.template Concat< StaticString("<")>()).template Concat< inner_left_arrow >()).template Concat<StaticString(">")>();
    } /*else if constexpr (left_arrow_index != -1 || right_arrow_index != -1) {
        static_assert(false, "Mismatched < and > in type name");
    } */ else {
        return StripClassOrStruct< Str >();
    }
}

// constexpr functions to strip namespaces from StaticString

template <auto Str>
constexpr auto StripNamespace()
{
    constexpr auto last_index = Str.template FindLast<containers::detail::IntegerSequenceFromString<StaticString("::")>>();

    if constexpr (last_index == -1) {
        return detail::StripClassOrStruct< Str >();
    } else {
        return detail::StripClassOrStruct< Str.template Substr<last_index + 2, Str.Size()>() >();
    }
}

template <auto Str>
constexpr auto StripNestedNamespace()
{
    constexpr auto left_arrow_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("<") > >();
    constexpr auto right_arrow_index = Str.template FindLast< containers::detail::IntegerSequenceFromString< StaticString(">") > >();

    if constexpr (left_arrow_index != SizeType(-1) && right_arrow_index != SizeType(-1)) {
        constexpr auto before_left_arrow = StripNamespace< Str.template Substr<0, left_arrow_index>() >();
        constexpr auto inner_left_arrow = StripNestedNamespace< Str.template Substr<left_arrow_index + 1, right_arrow_index>() >();
        // constexpr auto after_right_arrow = Str.template Substr<right_arrow_index + 1, Str.Size() - 1>();
        return ((before_left_arrow.template Concat< StaticString("<")>()).template Concat< inner_left_arrow >()).template Concat<StaticString(">")>();
    }/* else if constexpr (left_arrow_index != -1 || right_arrow_index != -1) {
        static_assert(false, "Mismatched < and > in type name");
    } */ else {
        return StripNamespace< Str >();
    }
}

} // namespace detail

/*! \brief Returns the name of the type T as a StaticString.
 *
 *  \tparam T The type to get the name of.
 *
 *  \return The name of the type T as a StaticString.
 */
template <class T>
constexpr auto TypeName()
{
#ifdef HYP_CLANG_OR_GCC
#ifdef HYP_CLANG
    constexpr StaticString<sizeof(__PRETTY_FUNCTION__)> name(__PRETTY_FUNCTION__);

    // auto hyperion::TypeName() [T = hyperion::Task<int, int>]
    constexpr auto substr = name.template Substr<31, sizeof(__PRETTY_FUNCTION__) - 2>();
#elif defined(HYP_GCC)
    constexpr StaticString<sizeof(__PRETTY_FUNCTION__)> name(__PRETTY_FUNCTION__);

    // constexpr auto hyperion::TypeName() [with T = hyperion::Task<int, int>]
    constexpr auto substr = name.template Substr<47, sizeof(__PRETTY_FUNCTION__) - 2>();
#endif
#elif defined(HYP_MSVC)
    constexpr StaticString<sizeof(__FUNCSIG__)> name(__FUNCSIG__);

    //  auto __cdecl hyperion::TypeName<class hyperion::Task<int,int>>(void)
    constexpr auto substr = name.template Substr<32, sizeof(__FUNCSIG__) - 8>();

#else
    static_assert(false, "Unsupported compiler for TypeName()");
#endif

    return detail::StripNestedClassOrStruct< substr >();
}

/*! \brief Returns the name of the type T as a StaticString. Removes the namespace from the name (e.g. hyperion::Task<int, int> -> Task<int, int>).
 *
 *  \tparam T The type to get the name of.
 *
 *  \return The name of the type T as a StaticString.
 */
template <class T>
constexpr auto TypeNameWithoutNamespace()
{
#ifdef HYP_CLANG_OR_GCC
#ifdef HYP_CLANG
    constexpr StaticString<sizeof(__PRETTY_FUNCTION__)> name(__PRETTY_FUNCTION__);

    // auto hyperion::TypeNameWithoutNamespace() [T = hyperion::Task<int, int>]
    constexpr auto substr = name.template Substr<47, sizeof(__PRETTY_FUNCTION__) - 2>();
#elif defined(HYP_GCC)
    constexpr StaticString<sizeof(__PRETTY_FUNCTION__)> name(__PRETTY_FUNCTION__);

    // constexpr auto hyperion::TypeNameWithoutNamespace() [with T = hyperion::Task<int, int>]
    constexpr auto substr = name.template Substr<63, sizeof(__PRETTY_FUNCTION__) - 2>();
#endif
#elif defined(HYP_MSVC)
    constexpr StaticString<sizeof(__FUNCSIG__)> name(__FUNCSIG__);

    //  auto __cdecl hyperion::TypeNameWithoutNamespace<class hyperion::Task<int,int>>(void)
    constexpr auto substr = name.template Substr<48, sizeof(__FUNCSIG__) - 8>();
#else
    static_assert(false, "Unsupported compiler for TypeNameWithoutNamespace()");
#endif
    
    return detail::StripNestedNamespace< substr >();
}

/*! \brief Size of an array literal (Hyperion equivalent of std::size) */
template <class T, uint N>
constexpr uint ArraySize(const T (&)[N])
{
    return N;
}

template <class T>
HYP_FORCE_INLINE
void Swap(T &a, T &b)
{
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>, "Swap requires T to be move constructible and move assignable");

    T temp = Move(a);
    a = Move(b);
    b = Move(temp);
}

} // namespace hyperion

#endif
