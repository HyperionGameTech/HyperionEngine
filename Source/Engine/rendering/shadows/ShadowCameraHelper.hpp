/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/math/Vector3.hpp>
#include <Core/math/BoundingBox.hpp>

namespace Hyperion {

class Camera;

class ShadowCameraHelper
{
public:
    static HYP_API void UpdateShadowCameraDirectional(
        const Handle<Camera>& camera,
        const Vec3f& center,
        const Vec3f& dir,
        float radius,
        BoundingBox& outAabb);
};

} // namespace Hyperion
