/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Utilities/Span.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/BoundingBox.hpp>

namespace Hyperion {

/// written on the sim thread, read by streaming workers
class TerrainLodViewpoints
{
public:
    void Set(Span<const Vec3f> viewpoints);

    Array<Vec3f> Get() const;

    /// infinity until the first Set()
    float GetNearestDistance(const BoundingBox& worldBounds) const;

private:
    mutable Mutex m_mutex;
    Array<Vec3f> m_viewpoints;
};

} // namespace Hyperion
