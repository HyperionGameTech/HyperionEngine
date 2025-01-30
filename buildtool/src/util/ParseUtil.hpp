/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_PARSE_UTIL_HPP
#define HYPERION_BUILDTOOL_PARSE_UTIL_HPP

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/Optional.hpp>

namespace hyperion {
namespace buildtool {

Optional<String> ExtractCXXClassName(const String &line);
Array<String> ExtractCXXBaseClasses(const String &line);

} // namespace buildtool
} // namespace hyperion

#endif
