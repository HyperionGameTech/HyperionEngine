#ifndef UTIL_H
#define UTIL_H

#include <util/Defines.hpp>
#include <system/Debug.hpp>
#include <Types.hpp>

#include <core/lib/StaticString.hpp>

namespace hyperion {

template <class T>
constexpr auto TypeName()
{
#ifdef HYP_CLANG_OR_GCC
#ifdef HYP_CLANG
    constexpr StaticString<sizeof(__PRETTY_FUNCTION__)> name(__PRETTY_FUNCTION__);

    // auto hyperion::TypeName() [T = hyperion::v2::Task<int, int>]
    return name.template Substr<31, sizeof(__PRETTY_FUNCTION__) - 2>();
#elif defined(HYP_GCC)
    constexpr StaticString<sizeof(__PRETTY_FUNCTION__)> name(__PRETTY_FUNCTION__);

    // constexpr auto hyperion::TypeName() [with T = hyperion::v2::Task<int, int>]
    return name.template Substr<47, sizeof(__PRETTY_FUNCTION__) - 2>();
#endif
#elif defined(HYP_MSVC)
    constexpr StaticString<sizeof(__FUNCSIG__)> name(__FUNCSIG__);

    // auto __cdecl hyperion::TypeName<hyperion::v2::Task<int,int>>(void) noexcept
    return name.template Substr<32, sizeof(__FUNCSIG__) - 16>();
#else
#error "Unsupported compiler for TypeName()"
#endif
}

} // namespace hyperion

#endif
