/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UTIL_HPP
#define HYPERION_UTIL_HPP

#include <core/utilities/ValueStorage.hpp>
#include <core/containers/StaticString.hpp>
#include <core/Defines.hpp>

#include <system/Debug.hpp>
#include <Types.hpp>

namespace hyperion {

namespace detail {

// constexpr function to strip namespace from StaticString
template <auto Str>
constexpr auto StripNamespace();

template <auto Str>
constexpr auto StripNestedNamespace()
{
    constexpr auto left_arrow_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("<") > >();
    constexpr auto right_arrow_index = Str.template FindLast< containers::detail::IntegerSequenceFromString< StaticString(">") > >();

    if constexpr (left_arrow_index != -1 && right_arrow_index != -1) {
        constexpr auto before_left_arrow = StripNestedNamespace< Str.template Substr<0, left_arrow_index>() >();
        constexpr auto inner_left_arrow = StripNestedNamespace< Str.template Substr<left_arrow_index + 1, right_arrow_index>() >();
        // constexpr auto after_right_arrow = Str.template Substr<right_arrow_index + 1, Str.Size() - 1>();
        return ((before_left_arrow.template Concat< StaticString("<")>()).template Concat< inner_left_arrow >()).template Concat<StaticString(">")>();
    } else {
        constexpr auto last_index = Str.template FindLast<containers::detail::IntegerSequenceFromString<StaticString("::")>>();

        if constexpr (last_index == -1) {
            return Str;
        } else {
            return Str.template Substr<last_index + 2, Str.Size() - 1>();
        }
    }
}

template <auto Str>
constexpr auto StripNamespace()
{
    constexpr auto last_index = Str.template FindLast<containers::detail::IntegerSequenceFromString<StaticString("::")>>();

    if constexpr (last_index == -1) {
        return StripNestedNamespace< Str >();
    } else {
        return StripNestedNamespace< Str.template Substr<last_index + 2, Str.Size() - 1>() >();
    }
}

} // namespace detail

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

    // auto hyperion::TypeName() [T = hyperion::Task<int, int>]
    constexpr auto substr = name.template Substr<47, sizeof(__PRETTY_FUNCTION__) - 2>();
#elif defined(HYP_GCC)
    constexpr StaticString<sizeof(__PRETTY_FUNCTION__)> name(__PRETTY_FUNCTION__);

    // constexpr auto hyperion::TypeName() [with T = hyperion::Task<int, int>]
    constexpr auto substr = name.template Substr<63, sizeof(__PRETTY_FUNCTION__) - 2>();
#endif
#elif defined(HYP_MSVC)
    constexpr StaticString<sizeof(__FUNCSIG__)> name(__FUNCSIG__);
    
    // auto hyperion::TypeName<hyperion::Task<int,int>>()
    constexpr auto substr = name.template Substr<24, sizeof(__FUNCSIG__) - 4>();
#else
    static_assert(false, "Unsupported compiler for TypeNameWithoutNamespace()");
#endif

    return detail::StripNamespace<substr>();
}

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

    // auto hyperion::TypeName<hyperion::Task<int,int>>()
    constexpr auto substr = name.template Substr<24, sizeof(__FUNCSIG__) - 4>();
#else
    static_assert(false, "Unsupported compiler for TypeName()");
#endif

    return substr;
}

/*! \brief Size of an array literal (Hyperion equivalent of std::size) */
template <class T, uint N>
constexpr uint ArraySize(const T (&)[N])
{
    return N;
}

} // namespace hyperion

#endif
