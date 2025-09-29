/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/Handle.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

class LightmapVolume;

HYP_STRUCT(Component)
struct LightmapVolumeComponent
{
    HYP_FIELD(Property = "Volume")
    Handle<LightmapVolume> volume;
};

} // namespace hyperion
