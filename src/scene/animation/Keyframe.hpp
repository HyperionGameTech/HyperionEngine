/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/Transform.hpp>
#include <core/Types.hpp>

namespace hyperion {

HYP_STRUCT()
struct HYP_API Keyframe
{
    HYP_STRUCT_BODY(Keyframe);

    HYP_FIELD(Property = "Time", Serialize = true)
    float time = 0.0f;

    HYP_FIELD(Property = "Transform", Serialize = true)
    Transform transform = Transform::identity;

    Keyframe() = default;
    Keyframe(float time, const Transform& transform)
        : time(time),
          transform(transform)
    {
    }

    Keyframe(const Keyframe& other) = default;
    Keyframe& operator=(const Keyframe& other) = default;

    Keyframe(Keyframe&& other) noexcept = default;
    Keyframe& operator=(Keyframe&& other) noexcept = default;

    ~Keyframe() = default;

    HYP_METHOD()
    Keyframe Blend(const Keyframe& to, float blend) const;
};

} // namespace hyperion
