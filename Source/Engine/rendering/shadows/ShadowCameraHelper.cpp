#include <RenderingPch.hpp>

#include <rendering/shadows/ShadowCameraHelper.hpp>

#include <scene/camera/Camera.hpp>

namespace Hyperion {

void ShadowCameraHelper::UpdateShadowCameraDirectional(
    Camera& camera,
    const Vec3f& center,
    const Vec3f& dir,
    float radius)
{
    camera.SetTranslation(center + (dir.Normalized() * -1.0f));
    camera.SetTarget(center);

    BoundingBox bounds { center - radius, center + radius };

    FixedArray<Vec3f, 8> corners = bounds.GetCorners();

    for (Vec3f& corner : corners)
    {
        corner = camera.GetViewMatrix() * corner;

        bounds.max = MathUtil::Max(bounds.max, corner);
        bounds.min = MathUtil::Min(bounds.min, corner);
    }

    camera.SetToOrthographicProjection(
        bounds.min.x, bounds.max.x,
        bounds.min.y, bounds.max.y,
        bounds.min.z, bounds.max.z);
}

} // namespace Hyperion
