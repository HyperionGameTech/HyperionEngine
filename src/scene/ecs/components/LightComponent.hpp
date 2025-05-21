/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_LIGHT_COMPONENT_HPP
#define HYPERION_ECS_LIGHT_COMPONENT_HPP

#include <core/Handle.hpp>

#include <HashCode.hpp>

namespace hyperion {

class Light;

HYP_STRUCT(Component, Size=16, Label="Light Component", Description="Controls the rendering of an object acting as a light source.", Editor=true)
struct LightComponent
{
    HYP_FIELD(Property="Light", Serialize=true, Editor=true)
    Handle<Light>   light;

    const Handle<Light> &GetLight() const
        { return light; }
    
    void SetLight(const Handle<Light> &light)
        { this->light = light; }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(light);

        return hash_code;
    }
};

} // namespace hyperion

#endif