/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_UTILS_HPP
#define HYPERION_CORE_HYP_CLASS_UTILS_HPP

#include <core/HypClass.hpp>
#include <core/HypClassProperty.hpp>

#include <Constants.hpp>

namespace hyperion {

#define HYP_DEFINE_CLASS(T, ...) \
    static ::hyperion::detail::HypClassRegistration<T> T##_Class { HypClassFlags::NONE, Span<HypClassProperty> { { __VA_ARGS__ } } }

#define HYP_DEFINE_STRUCT(T) \
    static_assert(IsPODType<NormalizedType<T>>, "HYP_DEFINE_STRUCT requires a POD type"); \
    static ::hyperion::detail::HypClassRegistration<T> T##_Class { HypClassFlags::POD_TYPE, Span<HypClassProperty> { { } } }

} // namespace hyperion

#endif