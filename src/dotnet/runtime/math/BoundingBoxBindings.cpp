/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/BoundingBox.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {
HYP_EXPORT float BoundingBox_GetRadius(BoundingBox *bounding_box)
{
    return bounding_box->GetRadius();
}

HYP_EXPORT bool BoundingBox_Contains(BoundingBox *left, BoundingBox *right)
{
    return left->Contains(*right);
}

HYP_EXPORT bool BoundingBox_ContainsPoint(BoundingBox *bounding_box, Vec3f *point)
{
    return bounding_box->ContainsPoint(*point);
}
} // extern "C"