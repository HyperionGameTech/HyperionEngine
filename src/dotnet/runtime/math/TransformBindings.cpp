#include <math/Transform.hpp>
#include <dotnet/runtime/math/ManagedMathTypes.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
HYP_EXPORT ManagedMatrix4 Transform_UpdateMatrix(ManagedTransform transform)
{
    Transform t(transform);
    t.UpdateMatrix();
    return t.GetMatrix();
}
} // extern "C"