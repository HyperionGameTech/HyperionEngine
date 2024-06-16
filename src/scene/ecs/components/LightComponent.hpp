/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_LIGHT_COMPONENT_HPP
#define HYPERION_ECS_LIGHT_COMPONENT_HPP

#include <core/Handle.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <rendering/Light.hpp>

#include <HashCode.hpp>

namespace hyperion {

enum class LightComponentFlags : uint32
{
    NONE    = 0x00
};

HYP_MAKE_ENUM_FLAGS(LightComponentFlags)

struct LightComponent
{
    Handle<Light>                   light;
    HashCode                        transform_hash_code;
    EnumFlags<LightComponentFlags>  flags = LightComponentFlags::NONE;

    const Handle<Light> &GetLight() const
        { return light; }
        
    void SetLight(const Handle<Light> &light)
        { this->light = light; }
};

static_assert(sizeof(LightComponent) == 24, "LightComponent size mismatch with C#");

} // namespace hyperion

#endif