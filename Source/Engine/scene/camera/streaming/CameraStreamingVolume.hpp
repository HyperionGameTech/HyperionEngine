/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <streaming/StreamingVolume.hpp>

#include <Core/reflection/Handle.hpp>
#include <Core/Defines.hpp>

#include <Core/math/Frustum.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class HYP_API CameraStreamingVolume : public StreamingVolumeBase
{
    HYP_OBJECT_BODY(CameraStreamingVolume);

public:
    CameraStreamingVolume() = default;
    virtual ~CameraStreamingVolume() override = default;

    void SetBoundingBox(const BoundingBox& aabb)
    {
        if (m_aabb == aabb)
        {
            return;
        }

        m_aabb = aabb;

        NotifyUpdate();
    }

protected:
    HYP_METHOD()
    virtual StreamingVolumeShape GetShape_Impl() const override
    {
        return StreamingVolumeShape::BOX; // Frustum is treated as a box for streaming purposes
    }

    HYP_METHOD()
    virtual bool GetBoundingBox_Impl(BoundingBox& outAabb) const override
    {
        outAabb = m_aabb;

        return true;
    }

    HYP_METHOD()
    virtual bool GetBoundingSphere_Impl(BoundingSphere& outSphere) const override
    {
        return false;
    }

    HYP_METHOD()
    virtual bool ContainsPoint_Impl(const Vec3f& point) const override
    {
        return m_aabb.ContainsPoint(point);
    }

    BoundingBox m_aabb;
};

} // namespace Hyperion
