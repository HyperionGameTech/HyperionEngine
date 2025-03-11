/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_ENUMS_HPP
#define HYPERION_FBOM_ENUMS_HPP

#include <core/utilities/EnumFlags.hpp>

#include <Types.hpp>

namespace hyperion {

enum class FBOMDataAttributes : uint8
{
    NONE                = 0x0,
    COMPRESSED          = 0x1,

    EXT_REF_PLACEHOLDER = 0x2,
    RESERVED1           = 0x4,
    RESERVED2           = 0x8,
    RESERVED3           = 0x10,

    LOCATION_MASK       = 0x20 | 0x40 | 0x80
};

HYP_MAKE_ENUM_FLAGS(FBOMDataAttributes)

enum class FBOMDataLocation : uint8
{
    LOC_STATIC  = 0,
    LOC_INPLACE,
    LOC_EXT_REF
};

enum FBOMCommand : uint8
{
    FBOM_NONE = 0,
    FBOM_OBJECT_START,
    FBOM_OBJECT_END,
    FBOM_STATIC_DATA_START,
    FBOM_STATIC_DATA_END,
    FBOM_STATIC_DATA_HEADER_START,
    FBOM_STATIC_DATA_HEADER_END,
    FBOM_DEFINE_PROPERTY,
    FBOM_OBJECT_LIBRARY_START,
    FBOM_OBJECT_LIBRARY_END
};

} // namespace hyperion

#endif