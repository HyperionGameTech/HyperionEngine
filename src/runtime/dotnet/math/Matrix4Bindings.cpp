#include <math/Matrix4.hpp>
#include <runtime/dotnet/math/ManagedMathTypes.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    ManagedMatrix4 Matrix4_Identity()
    {
        return Matrix4::Identity();
    }

    ManagedMatrix4 Matrix4_Multiply(ManagedMatrix4 left, ManagedMatrix4 right)
    {
        Matrix4 l(left);
        Matrix4 r(right);
        return l * r;
    }

    ManagedMatrix4 Matrix4_Inverted(ManagedMatrix4 matrix)
    {
        Matrix4 m(matrix);
        return m.Inverted();
    }

    ManagedMatrix4 Matrix4_Transposed(ManagedMatrix4 matrix)
    {
        Matrix4 m(matrix);
        return m.Transposed();
    }
}