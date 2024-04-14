#include <math/Transform.hpp>
#include <dotnet/runtime/math/ManagedMathTypes.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
HYP_EXPORT void Transform_UpdateMatrix(Transform *transform)
{
    transform->UpdateMatrix();
}
} // extern "C"