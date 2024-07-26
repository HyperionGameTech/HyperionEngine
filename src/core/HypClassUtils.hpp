/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_UTILS_HPP
#define HYPERION_CORE_HYP_CLASS_UTILS_HPP

#include <core/HypClass.hpp>
#include <core/HypClassProperty.hpp>

#include <Constants.hpp>

namespace hyperion {

#define HYP_DEFINE_CLASS(T, ...) \
    static ::hyperion::detail::HypClassRegistration<T> T##_Class { HypClassFlags::NONE, Span<HypClassProperty> { { __VA_ARGS__ } } }

} // namespace hyperion

#endif