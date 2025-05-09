/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SHADOWS_HPP
#define HYPERION_SHADOWS_HPP

#include <core/math/Vector2.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <Types.hpp>

namespace hyperion {

enum class ShadowMapFilterMode : uint32
{
    STANDARD,
    PCF,
    CONTACT_HARDENED,
    VSM,

    MAX
};

enum class ShadowFlags : uint32
{
    NONE                = 0x0,
    PCF                 = 0x1,
    VSM                 = 0x2,
    CONTACT_HARDENED    = 0x4
};

HYP_MAKE_ENUM_FLAGS(ShadowFlags)

enum class ShadowMapType : uint32
{
    DIRECTIONAL_SHADOW_MAP,
    SPOT_SHADOW_MAP,
    POINT_SHADOW_MAP,

    MAX
};

struct ShadowMapAtlasElement
{
    // Directional and spot lights only: index of the atlas in the shadow map texture array
    uint32  atlas_index = ~0u;

    // Point light shadow maps only: index of the cubemap in the texture array
    uint32  point_light_index = ~0u;

    // Offset in the atlas texture array, in uv space
    Vec2f   offset_uv;

    // Offset in the atlas texture array, in pixels
    Vec2u   offset_coords;

    // Dimensions of the shadow map in pixels
    Vec2u   dimensions;

    // Shadow map dimensions relative to the atlas dimensions
    Vec2f   scale;

    HYP_FORCE_INLINE bool operator==(const ShadowMapAtlasElement &other) const
    {
        return atlas_index == other.atlas_index
            && point_light_index == other.point_light_index
            && offset_uv == other.offset_uv
            && offset_coords == other.offset_coords
            && dimensions == other.dimensions
            && scale == other.scale;
    }

    HYP_FORCE_INLINE bool operator!=(const ShadowMapAtlasElement &other) const
    {
        return atlas_index != other.atlas_index
            || point_light_index != other.point_light_index
            || offset_uv != other.offset_uv
            || offset_coords != other.offset_coords
            || dimensions != other.dimensions
            || scale != other.scale;
    }
};


} // namespace hyperion

#endif