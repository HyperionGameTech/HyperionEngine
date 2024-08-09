/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_UTILS_HPP
#define HYPERION_CORE_HYP_CLASS_UTILS_HPP

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>

#include <Constants.hpp>

namespace hyperion {

#define HYP_DEFINE_CLASS(T, ...) \
    static ::hyperion::detail::HypClassRegistration<T> T##_Class { HypClassFlags::NONE, Span<HypProperty> { { __VA_ARGS__ } } }

} // namespace hyperion

#endif