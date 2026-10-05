/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/Math/Transform.hpp>

using namespace Hyperion;

extern "C"
{
    // not Transform_GetMatrix: that name is taken by the generated Strata thunk in core
    HYP_EXPORT void Transform_GetMatrixManaged(Transform* transform, Mat4f* outMatrix)
    {
        Assert(transform != nullptr && outMatrix != nullptr);

        *outMatrix = transform->GetMatrix();
    }
} // extern "C"
