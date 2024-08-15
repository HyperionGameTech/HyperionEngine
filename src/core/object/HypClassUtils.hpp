/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_UTILS_HPP
#define HYPERION_CORE_HYP_CLASS_UTILS_HPP

#include <core/object/HypClass.hpp>
#include <core/object/HypStruct.hpp>
#include <core/object/HypMember.hpp>

#include <Constants.hpp>

namespace hyperion {

#define HYP_DEFINE_CLASS(T, ...) \
    static ::hyperion::detail::HypClassRegistration<T> T##_ClassRegistration { HypClassFlags::CLASS_TYPE, Span<HypMember> { { __VA_ARGS__ } } }

// #define HYP_DEFINE_STRUCT(T, ...) \
//     static ::hyperion::detail::HypStructRegistration<T> T##_StructRegistration { HypClassFlags::STRUCT_TYPE, Span<HypMember> { { __VA_ARGS__ } } }

#define HYP_STRUCT_FIELD(name) HypField(NAME(HYP_STR(name)), &Type::min, offsetof(Type, name))

#define HYP_BEGIN_STRUCT(cls) static struct HypStructInitializer_##cls \
    { \
        using Type = cls; \
        static ::hyperion::detail::HypStructRegistration<Type> s_struct_registration; \
    } g_struct_initializer_##cls { }; \
    ::hyperion::detail::HypStructRegistration<HypStructInitializer_##cls::Type> HypStructInitializer_##cls::s_struct_registration { HypClassFlags::STRUCT_TYPE, Span<HypMember> { {

#define HYP_END_STRUCT } } };

} // namespace hyperion

#endif