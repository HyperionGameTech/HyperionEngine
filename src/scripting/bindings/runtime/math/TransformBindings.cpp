/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/math/Transform.hpp>

using namespace hyperion;

extern "C"
{
    HYP_EXPORT void Transform_GetMatrix(Transform* transform, Mat4f* outMatrix)
    {
        Assert(transform != nullptr && outMatrix != nullptr);

        *outMatrix = transform->GetMatrix();
    }
} // extern "C"
