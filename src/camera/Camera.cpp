#include "Camera.hpp"

#include <Engine.hpp>

namespace hyperion::v2 {

Camera::Camera(CameraType camera_type, int width, int height, float _near, float _far)
    : EngineComponentBase(),
      HasDrawProxy(),
      m_camera_type(camera_type),
      m_width(width),
      m_height(height),
      m_near(_near),
      m_far(_far),
      m_translation(Vector3::Zero()),
      m_direction(Vector3::UnitZ()),
      m_up(Vector3::UnitY()),
      m_command_queue_count{0}
{
}

Camera::~Camera()
{
    Teardown();
}

void Camera::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        HYP_FLUSH_RENDER_QUEUE(GetEngine());
    });
}

void Camera::SetTranslation(const Vector3 &translation)
{
    m_translation      = translation;
    m_next_translation = translation;

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetNextTranslation(const Vector3 &translation)
{
    m_next_translation = translation;
}

void Camera::SetDirection(const Vector3 &direction)
{
    m_direction = direction;

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetUpVector(const Vector3 &up)
{
    m_up = up;

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::Rotate(const Vector3 &axis, float radians)
{
    m_direction.Rotate(axis, radians);
    m_direction.Normalize();

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetViewMatrix(const Matrix4 &view_mat)
{
    m_view_mat = view_mat;

    UpdateViewProjectionMatrix();
}

void Camera::SetProjectionMatrix(const Matrix4 &proj_mat)
{
    m_proj_mat = proj_mat;

    UpdateViewProjectionMatrix();
}

void Camera::SetViewProjectionMatrix(const Matrix4 &view_mat, const Matrix4 &proj_mat)
{
    m_view_mat = view_mat;
    m_proj_mat = proj_mat;

    UpdateViewProjectionMatrix();
}

void Camera::UpdateViewProjectionMatrix()
{
    m_view_proj_mat = m_proj_mat * m_view_mat;

    m_frustum.SetFromViewProjectionMatrix(m_view_proj_mat);
}

Vector3 Camera::TransformScreenToNDC(const Vector2 &screen) const
{
    return {
        1.0f - (2.0f * screen.x),
        1.0f - (2.0f * screen.y),
        1.0f
    };
}

Vector4 Camera::TransformNDCToWorld(const Vector3 &ndc) const
{
    Vector4 clip(ndc.x, ndc.y, -1.0f, 1.0f);

    Vector4 eye = m_proj_mat.Inverted() * clip;
    eye = Vector4(eye.x, eye.y, -1.0f, 0.0f);

    return m_view_mat.Inverted() * eye;
}

Vector3 Camera::TransformWorldToNDC(const Vector3 &world) const
{
    return m_view_proj_mat * world;
}

Vector2 Camera::TransformNDCToScreen(const Vector3 &ndc) const
{
    return {
        (0.5f * ndc.x) + 0.5f,
        (0.5f * ndc.y) + 0.5f
    };
}

Vector4 Camera::TransformScreenToWorld(const Vector2 &screen) const
{
    return TransformNDCToWorld(TransformScreenToNDC(screen));
}

void Camera::Update(Engine *engine, GameCounter::TickUnit dt)
{
    UpdateCommandQueue(dt);
    UpdateLogic(dt);

    m_translation = m_next_translation;

    UpdateMatrices();

    // enqueue render update to update the draw_proxy proxy object
    EnqueueDrawProxyUpdate(engine, CameraDrawProxy {
        .view       = m_view_mat,
        .projection = m_proj_mat,
        .position   = m_translation,
        .direction  = m_direction,
        .dimensions = { static_cast<UInt>(m_width), static_cast<UInt>(m_height) },
        .clip_near  = m_near,
        .clip_far   = m_far,
        .fov        = m_fov
    });
}

void Camera::UpdateMatrices()
{
    UpdateViewMatrix();
    UpdateProjectionMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::PushCommand(const CameraCommand &command)
{
    std::lock_guard guard(m_command_queue_mutex);

    ++m_command_queue_count;

    m_command_queue.Push(command);
}

void Camera::UpdateCommandQueue(GameCounter::TickUnit dt)
{
    if (m_command_queue_count == 0) {
        return;
    }

    std::lock_guard guard(m_command_queue_mutex);

    while (m_command_queue.Any()) {
        RespondToCommand(m_command_queue.Front(), dt);

        m_command_queue.Pop();
    }

    m_command_queue_count = 0;
}

} // namespace hyperion::v2
