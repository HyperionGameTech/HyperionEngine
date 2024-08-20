/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_UTILS_HPP
#define HYPERION_CORE_HYP_CLASS_UTILS_HPP

#include <core/object/HypClass.hpp>
#include <core/object/HypStruct.hpp>
#include <core/object/HypMember.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <Constants.hpp>

namespace hyperion {

#define HYP_DEFINE_CLASS(T, ...) \
    static ::hyperion::detail::HypClassRegistration<T, HypClassFlags::CLASS_TYPE> T##_ClassRegistration { NAME(HYP_STR(T)), {}, {}, Span<HypMember> { { __VA_ARGS__ } } }

// #define HYP_DEFINE_STRUCT(T, ...) \
//     static ::hyperion::detail::HypStructRegistration<T> T##_StructRegistration { HypClassFlags::STRUCT_TYPE, Span<HypMember> { { __VA_ARGS__ } } }

// #define HYP_FIELD(name) HypField(NAME(HYP_STR(name)), &Type::name, offsetof(Type, name))

// #define HYP_PROPERTY(name, ...) HypProperty(NAME(HYP_STR(name)), __VA_ARGS__)

// #define HYP_METHOD(name) HypMethod(NAME(HYP_STR(name)), &Type::name)
#define HYP_FUNCTION(name, fn) HypMethod(NAME(HYP_STR(name)), fn)

#define HYP_BEGIN_STRUCT(cls, ...) static struct HypStructInitializer_##cls \
    { \
        using Type = cls; \
        \
        using RegistrationType = ::hyperion::detail::HypStructRegistration<Type, HypClassFlags::STRUCT_TYPE>; \
        \
        static RegistrationType s_struct_registration; \
    } g_struct_initializer_##cls { }; \
    \
    HypStructInitializer_##cls::RegistrationType HypStructInitializer_##cls::s_struct_registration { NAME(HYP_STR(cls)), {}, {}, Span<HypMember> { {

#define HYP_END_STRUCT } } };

#define HYP_BEGIN_CLASS(cls, parent_class, ...) static struct HypClassInitializer_##cls \
    { \
        using Type = cls; \
        \
        using RegistrationType = ::hyperion::detail::HypClassRegistration<Type, HypClassFlags::CLASS_TYPE>; \
        \
        static RegistrationType s_class_registration; \
    } g_class_initializer_##cls { }; \
    \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_class_registration { NAME(HYP_STR(cls)), parent_class, Span<HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> { {

#define HYP_END_CLASS } } };

} // namespace hyperion

#endif