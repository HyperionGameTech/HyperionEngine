/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_TEMPLATES_HPP
#define HYPERION_BUILDTOOL_TEMPLATES_HPP

#include <core/containers/String.hpp>

namespace hyperion {
namespace buildtool {

struct HypMemberDefinition;

String MemberDummyClass(const HypMemberDefinition &definition);

} // namespace buildtool
} // namespace hyperion

#endif
