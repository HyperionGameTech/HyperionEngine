/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_UTILS_HPP
#define HYPERION_CORE_HYP_CLASS_UTILS_HPP

#include <core/object/HypClass.hpp>
#include <core/object/HypStruct.hpp>
#include <core/object/HypMember.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <Constants.hpp>

#include <type_traits>

namespace hyperion {

template <class T, class T2 = void>
struct DefaultHypClassFlags;

template <class T>
struct DefaultHypClassFlags<T, std::enable_if_t<std::is_class_v<T>>>
{
    static constexpr EnumFlags<HypClassFlags> value = (IsPODType<T> ? HypClassFlags::POD_TYPE : HypClassFlags::NONE)
        | (std::is_abstract_v<T> ? HypClassFlags::ABSTRACT : HypClassFlags::NONE);
};

#define HYP_FUNCTION(name, fn) HypMethod(NAME(HYP_STR(name)), fn)


#define HYP_BEGIN_STRUCT(cls, ...) static struct HypClassInitializer_##cls \
    { \
        using Type = cls; \
        \
        using RegistrationType = ::hyperion::detail::HypStructRegistration<Type, HypClassFlags::STRUCT_TYPE | DefaultHypClassFlags<Type>::value>; \
        \
        static RegistrationType s_class_registration; \
    } g_class_initializer_##cls { }; \
    \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_class_registration { NAME(HYP_STR(cls)), Span<const HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> { {

#define HYP_END_STRUCT } } };

#define HYP_BEGIN_CLASS(cls, parent_class, ...) static struct HypClassInitializer_##cls \
    { \
        using Type = cls; \
        \
        using RegistrationType = ::hyperion::detail::HypClassRegistration<Type, HypClassFlags::CLASS_TYPE | DefaultHypClassFlags<Type>::value>; \
        \
        static RegistrationType s_class_registration; \
    } g_class_initializer_##cls { }; \
    \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_class_registration { NAME(HYP_STR(cls)), parent_class, Span<const HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> { {

#define HYP_END_CLASS } } };

} // namespace hyperion

#endif