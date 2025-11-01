/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/camera/FollowCamera.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

FollowCameraController::FollowCameraController(const Vector3& target, const Vector3& offset)
    : PerspectiveCameraController(),
      m_target(target),
      m_offset(offset),
      m_realOffset(offset),
      m_mx(0.0f),
      m_my(0.0f),
      m_prevMx(0.0f),
      m_prevMy(0.0f),
      m_desiredDistance(target.Distance(offset))
{
}

void FollowCameraController::OnActivated()
{
    HYP_SCOPE;

    PerspectiveCameraController::OnActivated();

    m_camera->SetTarget(m_target);
}

void FollowCameraController::OnDeactivated()
{
    HYP_SCOPE;

    PerspectiveCameraController::OnDeactivated();
}

void FollowCameraController::UpdateLogic(double dt)
{
    HYP_SCOPE;

    m_realOffset.Lerp(m_offset, MathUtil::Clamp(float(dt) * 25.0f, 0.0f, 1.0f));

    const Vector3 origin = m_camera->GetTarget();
    const Vector3 normalizedOffsetDirection = (origin - (origin + m_realOffset)).Normalized();

    m_camera->SetTranslation(origin + normalizedOffsetDirection * m_desiredDistance);
}

void FollowCameraController::RespondToCommand(const CameraCommand& command, float dt)
{
    HYP_SCOPE;

    switch (command.command)
    {
    case CameraCommand::CAMERA_COMMAND_MAG:
    {
        m_mx = command.magData.mx;
        m_my = command.magData.my;

        m_mag = {
            m_mx - m_prevMx,
            m_my - m_prevMy
        };

        constexpr float mouseSpeed = 80.0f;

        m_offset = Vector3(
            -MathUtil::Sin(m_mag.x * 4.0f) * mouseSpeed,
            -MathUtil::Sin(m_mag.y * 4.0f) * mouseSpeed,
            MathUtil::Cos(m_mag.x * 4.0f) * mouseSpeed);

        break;
    }
    case CameraCommand::CAMERA_COMMAND_SCROLL:
    {
        constexpr float scrollSpeed = 150.0f;

        m_desiredDistance -= float(command.scrollData.wheelY) * scrollSpeed * dt;

        break;
    }
    case CameraCommand::CAMERA_COMMAND_MOVEMENT:
    {
        constexpr float movementSpeed = 80.0f;
        const float speed = movementSpeed * dt;

        const Vector3 dirCrossY = Vector3(m_camera->m_direction).Cross(m_camera->m_up);

        switch (command.movementData.movementType)
        {
        case CameraCommand::CAMERA_MOVEMENT_FORWARD:
            m_offset -= m_camera->m_up * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_BACKWARD:
            m_offset += m_camera->m_up * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_LEFT:
            m_offset += dirCrossY * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_RIGHT:
            m_offset -= dirCrossY * speed;

            break;
        default:
            break;
        }

        break;
    }
    default:
        break;
    }
}

} // namespace hyperion
