/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/math/Vector3.hpp>
#include <Core/math/BoundingBox.hpp>

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
