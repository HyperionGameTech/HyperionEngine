/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/math/Mat3f.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT void Matrix3_Multiply(Mat3f* left, Mat3f* right, Mat3f* result)
    {
        *result = *left * *right;
    }

    HYP_EXPORT void Matrix3_Inverse(Mat3f* in, Mat3f* result)
    {
        *result = (*in).Inverse();
    }

    HYP_EXPORT void Matrix3_Transpose(Mat3f* in, Mat3f* result)
    {
        *result = (*in).Transpose();
    }
} // extern "C"
