/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/math/Transform.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT void Transform_GetMatrix(Transform* transform, Mat4f* outMatrix)
    {
        Assert(transform != nullptr && outMatrix != nullptr);

        *outMatrix = transform->GetMatrix();
    }
} // extern "C"
