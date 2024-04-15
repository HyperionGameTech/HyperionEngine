/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_SHADOWS_HPP
#define HYPERION_V2_SHADOWS_HPP

#include <Types.hpp>

namespace hyperion::v2 {

enum class ShadowMode : uint
{
    STANDARD,
    PCF,
    CONTACT_HARDENED,
    VSM,

    MAX
};

} // namespace hyperion::v2

#endif