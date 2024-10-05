/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_SHADOW_MAP_COMPONENT_HPP
#define HYPERION_ECS_SHADOW_MAP_COMPONENT_HPP

#include <core/Handle.hpp>
#include <core/Name.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <scene/camera/Camera.hpp>

#include <math/Extent.hpp>
#include <math/Vector2.hpp>
#include <math/Matrix4.hpp>
#include <math/BoundingBox.hpp>

#include <rendering/Shadows.hpp>

#include <HashCode.hpp>

namespace hyperion {

class RenderComponentBase;

HYP_STRUCT(Component)
struct ShadowMapComponent
{
    HYP_FIELD(Serialize, Property="Mode")
    ShadowMode              mode = ShadowMode::STANDARD;

    HYP_FIELD(Serialize, Property="Radius")
    float                   radius = 20.0f;

    HYP_FIELD(Serialize, Property="Resolution")
    Vec2u                   resolution = Vec2u { 512, 512 };

    HYP_FIELD()
    RC<RenderComponentBase> render_component;

    HYP_FIELD()
    uint32                  update_counter = 0;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;
        hash_code.Add(mode);
        hash_code.Add(radius);
        hash_code.Add(resolution);

        return hash_code;
    }
};

static_assert(sizeof(ShadowMapComponent) == 32, "ShadowMapComponent size mismatch with C#");

} // namespace hyperion

#endif
