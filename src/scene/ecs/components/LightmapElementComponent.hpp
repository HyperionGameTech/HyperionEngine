/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_LIGHTMAP_ELEMENT_COMPONENT_HPP
#define HYPERION_ECS_LIGHTMAP_ELEMENT_COMPONENT_HPP

#include <core/Handle.hpp>

#include <core/utilities/UUID.hpp>

#include <Types.hpp>

namespace hyperion {

class LightmapVolume;

HYP_STRUCT(Component)
struct LightmapElementComponent
{
    HYP_FIELD(Property="ElementIndex", Serialize=true)
    uint32                      element_index = ~0u;

    HYP_FIELD(Property="VolumeUUID", Serialize=true)
    UUID                        volume_uuid = UUID::Invalid();

    HYP_FIELD(Property="Volume", Serialize=false)
    WeakHandle<LightmapVolume>  volume;
};

} // namespace hyperion

#endif