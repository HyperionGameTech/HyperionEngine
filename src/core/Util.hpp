#ifndef UTIL_H
#define UTIL_H

#include <util/Defines.hpp>
#include <system/Debug.hpp>
#include <Types.hpp>

#include <core/lib/StaticString.hpp>

namespace hyperion {

namespace detail {

// constexpr function to strip namespace from StaticString

template <auto Str>
constexpr auto StripNamespace()
{
    constexpr auto left_arrow_index = Str.template FindFirst< IntegerSequenceFromString< StaticString("<") > >();
    constexpr auto right_arrow_index = Str.template FindFirst< IntegerSequenceFromString< StaticString(">") > >();

    if constexpr (left_arrow_index != -1 && right_arrow_index != -1) {
        constexpr auto before_left_arrow = StripNamespace< Str.template Substr<0, left_arrow_index>() >();
        constexpr auto inner_left_arrow = StripNamespace< Str.template Substr<left_arrow_index + 1, right_arrow_index>() >();
        // constexpr auto after_right_arrow = Str.template Substr<right_arrow_index + 1, Str.Size() - 1>();
        return ((before_left_arrow.template Concat< StaticString("<")>()).template Concat< inner_left_arrow >()).template Concat<StaticString(">")>();
    } else {
        constexpr auto last_index = Str.template FindLast<IntegerSequenceFromString<StaticString("::")>>();

        if constexpr (last_index == -1) {
            return Str;
        } else {
            return Str.template Substr<last_index + 2, Str.Size() - 1>();
        }
    }
}

} // namespace detail

/*! \brief Returns the name of the type T as a StaticString. Removes the namespace from the name (e.g. hyperion::v2::Task<int, int> -> Task<int, int>).
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

    // auto hyperion::TypeName() [T = hyperion::v2::Task<int, int>]
    constexpr auto substr = name.template Substr<31, sizeof(__PRETTY_FUNCTION__) - 2>();
#elif defined(HYP_GCC)
    constexpr StaticString<sizeof(__PRETTY_FUNCTION__)> name(__PRETTY_FUNCTION__);

    // constexpr auto hyperion::TypeName() [with T = hyperion::v2::Task<int, int>]
    constexpr auto substr = name.template Substr<47, sizeof(__PRETTY_FUNCTION__) - 2>();
#endif
#elif defined(HYP_MSVC)
    constexpr StaticString<sizeof(__FUNCSIG__)> name(__FUNCSIG__);

    // auto __cdecl hyperion::TypeName<hyperion::v2::Task<int,int>>(void) noexcept
    constexpr auto substr = name.template Substr<32, sizeof(__FUNCSIG__) - 16>();
#else
    static_assert(false, "Unsupported compiler for TypeName()");
#endif

    // // Remove the namespace from the name
    // // @FIXME: Will not work with templates currently
    // constexpr auto last_index = substr.template FindLast<IntegerSequenceFromString<StaticString("::")>>();

    // if constexpr (last_index == -1) {
    //     return substr;
    // } else {
    //     return substr.template Substr<last_index + 2, substr.Size() - 1>();
    // }

    return detail::StripNamespace<substr>();

    // return substr;
}

} // namespace hyperion

#endif
