/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/math/Mat4f.hpp>

using namespace Hyperion;

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

    HYP_EXPORT void Matrix4_Inverse(Mat4f* in, Mat4f* result)
    {
        *result = (*in).Inverse();
    }

    HYP_EXPORT void Matrix4_Transpose(Mat4f* in, Mat4f* result)
    {
        *result = (*in).Transpose();
    }
} // extern "C"
