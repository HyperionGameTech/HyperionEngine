/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <math/Matrix4.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {
HYP_EXPORT void Matrix4_Identity(Matrix4 *matrix)
{
    *matrix = Matrix4::Identity();
}

HYP_EXPORT void Matrix4_Multiply(Matrix4 *left, Matrix4 *right, Matrix4 *result)
{
    *result = *left * *right;
}

HYP_EXPORT void Matrix4_Inverted(Matrix4 *in, Matrix4 *result)
{
    *result = (*in).Inverted();
}

HYP_EXPORT void Matrix4_Transposed(Matrix4 *in, Matrix4 *result)
{
    *result = (*in).Transposed();
}
} // extern "C"