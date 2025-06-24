/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_SHADOW_MAP_COMPONENT_HPP
#define HYPERION_ECS_SHADOW_MAP_COMPONENT_HPP

#include <core/Name.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <scene/camera/Camera.hpp>

#include <core/math/Extent.hpp>
#include <core/math/Vector2.hpp>
#include <core/math/Matrix4.hpp>
#include <core/math/BoundingBox.hpp>

#include <rendering/RenderShadowMap.hpp>

#include <HashCode.hpp>

namespace hyperion {

class Subsystem;

HYP_STRUCT(Component, Label = "Shadow Map Component", Description = "Controls shadow map rendering for a light source.", Editor = true, Size = 32)
struct ShadowMapComponent
{
    HYP_FIELD(Property = "Mode", Serialize = true, Editor = true)
    ShadowMapFilter mode = SMF_STANDARD;

    HYP_FIELD(Property = "Radius", Serialize = true, Editor = true)
    float radius = 20.0f;

    HYP_FIELD(Property = "Resolution", Serialize = true, Editor = true)
    Vec2u resolution = Vec2u { 512, 512 };

    HYP_FIELD()
    Handle<Subsystem> subsystem;

    HYP_FIELD()
    uint32 update_counter = 0;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;
        hash_code.Add(mode);
        hash_code.Add(radius);
        hash_code.Add(resolution);

        return hash_code;
    }
};

} // namespace hyperion

#endif
