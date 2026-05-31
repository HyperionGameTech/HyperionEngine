/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/BoundingBox.hpp>

namespace Hyperion {

class Camera;

class ShadowCameraHelper
{
public:
    static void UpdateShadowCameraDirectional(
        Camera& camera,
        const Vec3f& center,
        const Vec3f& dir,
        float radius);
};

} // namespace Hyperion
