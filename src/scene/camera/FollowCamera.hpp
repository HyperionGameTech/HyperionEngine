/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_FOLLOW_CAMERA_H
#define HYPERION_V2_FOLLOW_CAMERA_H

#include "PerspectiveCamera.hpp"

namespace hyperion::v2 {
class FollowCameraController : public PerspectiveCameraController
{
public:
    FollowCameraController(const Vector3 &target, const Vector3 &offset);
    virtual ~FollowCameraController() override = default;

    const Vector3 &GetOffset() const { return m_offset; }
    void SetOffset(const Vector3 &offset) { m_offset = offset; }

    virtual void OnAdded(Camera *camera) override;

    virtual void UpdateLogic(double dt) override;

private:
    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) override;

    Vector3 m_offset,
        m_real_offset;

    Vector3 m_target;

    float m_mx,
        m_my,
        m_prev_mx,
        m_prev_my,
        m_desired_distance;
    
    Vector2 m_mag,
        m_prev_mag;
};
} // namespace hyperion::v2

#endif
