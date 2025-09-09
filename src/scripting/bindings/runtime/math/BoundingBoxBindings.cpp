/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/BoundingBox.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{
    HYP_EXPORT float BoundingBox_GetRadius(BoundingBox* boundingBox)
    {
        return boundingBox->GetRadius();
    }

    HYP_EXPORT bool BoundingBox_Contains(BoundingBox* left, BoundingBox* right)
    {
        return left->Contains(*right);
    }

    HYP_EXPORT bool BoundingBox_ContainsPoint(BoundingBox* boundingBox, Vec3f* point)
    {
        return boundingBox->ContainsPoint(*point);
    }
} // extern "C"