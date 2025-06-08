/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CAMERA_STREAMING_VOLUME_HPP
#define HYPERION_CAMERA_STREAMING_VOLUME_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>
#include <core/Defines.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Frustum.hpp>

#include <streaming/StreamingManager.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class HYP_API CameraStreamingVolume : public StreamingVolumeBase
{
    HYP_OBJECT_BODY(CameraStreamingVolume);

public:
    CameraStreamingVolume() = default;
    virtual ~CameraStreamingVolume() override = default;

    HYP_FORCE_INLINE void SetBoundingBox(const BoundingBox &aabb)
    {
        m_aabb = aabb;
    }
    
protected:
    HYP_METHOD()
    virtual StreamingVolumeShape GetShape_Impl() const override
    {
        return StreamingVolumeShape::BOX; // Frustum is treated as a box for streaming purposes
    }

    HYP_METHOD()
    virtual bool GetBoundingBox_Impl(BoundingBox& out_aabb) const override
    {
        out_aabb = m_aabb;

        return true;
    }

    HYP_METHOD()
    virtual bool GetBoundingSphere_Impl(BoundingSphere& out_sphere) const override
    {
        return false;
    }

    HYP_METHOD()
    virtual bool ContainsPoint_Impl(const Vec3f &point) const override
    {
        return m_aabb.ContainsPoint(point);
    }

    BoundingBox m_aabb;
};

} // namespace hyperion

#endif
