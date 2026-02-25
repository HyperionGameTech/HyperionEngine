/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <Core/math/BoundingBox.hpp>

using namespace Hyperion;

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
