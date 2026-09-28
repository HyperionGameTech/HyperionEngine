/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainLodViewpoints.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {

void TerrainLodViewpoints::Set(Span<const Vec3f> viewpoints)
{
    AssertOnThread(g_simThread);

    Mutex::Guard guard(m_mutex);

    m_viewpoints.Resize(viewpoints.Size());

    for (size_t viewpointIndex = 0; viewpointIndex < viewpoints.Size(); viewpointIndex++)
    {
        m_viewpoints[viewpointIndex] = viewpoints[viewpointIndex];
    }
}

Array<Vec3f> TerrainLodViewpoints::Get() const
{
    Mutex::Guard guard(m_mutex);

    return m_viewpoints;
}

float TerrainLodViewpoints::GetNearestDistance(const BoundingBox& worldBounds) const
{
    Mutex::Guard guard(m_mutex);

    float nearestDistance = MathUtil::Infinity<float>();

    for (const Vec3f& viewpoint : m_viewpoints)
    {
        const Vec3f closestPoint {
            MathUtil::Clamp(viewpoint.x, worldBounds.min.x, worldBounds.max.x),
            MathUtil::Clamp(viewpoint.y, worldBounds.min.y, worldBounds.max.y),
            MathUtil::Clamp(viewpoint.z, worldBounds.min.z, worldBounds.max.z)
        };

        nearestDistance = MathUtil::Min(nearestDistance, (viewpoint - closestPoint).Length());
    }

    return nearestDistance;
}

} // namespace Hyperion
