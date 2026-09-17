/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/BoundingSphere.hpp>

#include <Core/Memory/Allocator/ArenaAllocator.hpp>

#include <Framework/EngineMemory.hpp>

namespace Hyperion {

class Camera;

struct LODViewData
{
    Vec3f position;

    float projectionScale = 1.0f;
    float nearClip = 0.01f;

    bool isOrthographic = false;

    LODViewData() = default;
    explicit LODViewData(const Camera& camera);

    inline float ComputeScreenSize(const BoundingSphere& sphere) const
    {
        if (sphere.radius <= 0.0f)
        {
            return 0.0f;
        }

        if (isOrthographic)
        {
            return sphere.radius * projectionScale;
        }

        const float distance = MathUtil::Max(position.Distance(sphere.center), nearClip);

        return sphere.radius * projectionScale / distance;
    }
};

} // namespace Hyperion
