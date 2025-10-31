/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <streaming/StreamingVolume.hpp>

#include <core/reflection/Handle.hpp>
#include <core/Defines.hpp>

#include <core/math/Frustum.hpp>

namespace hyperion {

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

} // namespace hyperion
