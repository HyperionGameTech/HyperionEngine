/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/Mat4f.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{
    HYP_EXPORT void Matrix4_Identity(Mat4f* matrix)
    {
        *matrix = Mat4f::Identity();
    }

    HYP_EXPORT void Matrix4_Multiply(Mat4f* left, Mat4f* right, Mat4f* result)
    {
        *result = *left * *right;
    }

    HYP_EXPORT void Matrix4_Inverted(Mat4f* in, Mat4f* result)
    {
        *result = (*in).Inverted();
    }

    HYP_EXPORT void Matrix4_Transposed(Mat4f* in, Mat4f* result)
    {
        *result = (*in).Transposed();
    }
} // extern "C"