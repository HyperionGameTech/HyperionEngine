#include <math/Matrix4.hpp>
#include <dotnet/runtime/math/ManagedMathTypes.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
HYP_EXPORT ManagedMatrix4 Matrix4_Identity()
{
    return Matrix4::Identity();
}

HYP_EXPORT ManagedMatrix4 Matrix4_Multiply(ManagedMatrix4 left, ManagedMatrix4 right)
{
    Matrix4 l(left);
    Matrix4 r(right);
    return l * r;
}

HYP_EXPORT ManagedMatrix4 Matrix4_Inverted(ManagedMatrix4 matrix)
{
    Matrix4 m(matrix);
    return m.Inverted();
}

HYP_EXPORT ManagedMatrix4 Matrix4_Transposed(ManagedMatrix4 matrix)
{
    Matrix4 m(matrix);
    return m.Transposed();
}
} // extern "C"