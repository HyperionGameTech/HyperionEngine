/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_ENUMS_HPP
#define HYPERION_FBOM_ENUMS_HPP

#include <core/utilities/EnumFlags.hpp>

#include <Types.hpp>

namespace hyperion {

enum class FBOMDataAttributes : uint8
{
    NONE            = 0x0,
    COMPRESSED      = 0x1,

    RESERVED0       = 0x2,
    RESERVED1       = 0x4,
    RESERVED2       = 0x8,
    RESERVED3       = 0x10,

    LOCATION_MASK   = 0x20 | 0x40 | 0x80
};

HYP_MAKE_ENUM_FLAGS(FBOMDataAttributes)

} // namespace hyperion

#endif