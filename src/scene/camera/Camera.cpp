#include "Camera.hpp"
#include <math/Halton.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Result;

class Camera;

struct RENDER_COMMAND(UpdateCameraDrawProxy) : renderer::RenderCommand
{
    Camera *camera;
    CameraDrawProxy draw_proxy;

    RENDER_COMMAND(UpdateCameraDrawProxy)(Camera *camera, const CameraDrawProxy &draw_proxy)
        : camera(camera),
          draw_proxy(draw_proxy)
    {
    }

    virtual Result operator()()
    {
        camera->m_draw_proxy = draw_proxy;

        CameraShaderData &shader_data = g_engine->GetRenderData()->cameras.Get(camera->GetID().ToIndex());

        shader_data.view             = draw_proxy.view;
        shader_data.projection       = draw_proxy.projection;
        shader_data.previous_view    = draw_proxy.previous_view;
        shader_data.dimensions       = { draw_proxy.dimensions.width, draw_proxy.dimensions.height, 0, 1 };
        shader_data.camera_position  = Vector4(draw_proxy.position, 1.0f);
        shader_data.camera_direction = Vector4(draw_proxy.position, 1.0f);
        shader_data.camera_near      = draw_proxy.clip_near;
        shader_data.camera_fov       = draw_proxy.fov;
        shader_data.camera_far       = draw_proxy.clip_far;

        g_engine->GetRenderData()->cameras.MarkDirty(camera->GetID().ToIndex());
        
        HYPERION_RETURN_OK;
    }
};

static Matrix4 BuildJitterMatrix(const Camera &camera, UInt frame_counter)
{
    if (camera.GetWidth() == 0 || camera.GetHeight() == 0) {
        return Matrix4::identity;
    }

    static const HaltonSequence halton;

    const Vector2 pixel_size = Vector2::one / Vector2(Float(MathUtil::Abs(camera.GetWidth())), Float(MathUtil::Abs(camera.GetHeight())));
    const UInt index = frame_counter % HaltonSequence::size;

    const Vector2 jitter = halton.sequence[index] * 2.0f - 1.0f;

    Matrix4 jitter_matrix = camera.GetProjectionMatrix();
    jitter_matrix[0][2] += jitter.x * pixel_size.x;
    jitter_matrix[1][2] += jitter.y * pixel_size.y;

    return jitter_matrix;
}

CameraController::CameraController(CameraType type)
    : m_camera(nullptr),
      m_type(type),
      m_command_queue_count { 0 }
{
}

void CameraController::PushCommand(const CameraCommand &command)
{
    std::lock_guard guard(m_command_queue_mutex);

    ++m_command_queue_count;

    m_command_queue.Push(command);
}

void CameraController::UpdateCommandQueue(GameCounter::TickUnit dt)
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

Camera::Camera()
    : Camera(128, 128)
{
}

Camera::Camera(int width, int height)
    : BasicObject(),
      HasDrawProxy(),
      m_fov(50.0f),
      m_width(width),
      m_height(height),
      m_near(0.01f),
      m_far(1000.0f),
      m_left(0.0f),
      m_right(0.0f),
      m_bottom(0.0f),
      m_top(0.0f),
      m_translation(Vector3::Zero()),
      m_direction(Vector3::UnitZ()),
      m_up(Vector3::UnitY())
{
}

Camera::Camera(float fov, int width, int height, float _near, float _far)
    : BasicObject(),
      HasDrawProxy(),
      m_fov(fov),
      m_width(width),
      m_height(height),
      m_translation(Vector3::Zero()),
      m_direction(Vector3::UnitZ()),
      m_up(Vector3::UnitY())
{
    SetToPerspectiveProjection(fov, _near, _far);
}

Camera::Camera(int width, int height, float left, float right, float bottom, float top, float _near, float _far)
    : BasicObject(),
      HasDrawProxy(),
      m_fov(0.0f),
      m_width(width),
      m_height(height),
      m_translation(Vector3::Zero()),
      m_direction(Vector3::UnitZ()),
      m_up(Vector3::UnitY())
{
    SetToOrthographicProjection(left, right, bottom, top, _near, _far);
}

Camera::~Camera()
{
    Teardown();
}

void Camera::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    //AssertThrowMsg(m_width > 0 && m_width < 100000 && m_width > 0 && m_height < 100000, "Invalid camera size");

    m_draw_proxy = CameraDrawProxy {
        .view = m_view_mat,
        .projection = m_proj_mat,
        .previous_view = m_previous_view_matrix,
        .position = m_translation,
        .direction = m_direction,
        .up = m_up,
        .dimensions = Extent2D { UInt(MathUtil::Abs(m_width)), UInt(MathUtil::Abs(m_height)) },
        .clip_near = m_near,
        .clip_far = m_far,
        .fov = m_fov,
        .frustum = m_frustum
    };

    InitObject(m_framebuffer);

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        HYP_SYNC_RENDER();
    });
}

void Camera::SetFramebuffer(const Handle<Framebuffer> &framebuffer)
{
    m_framebuffer = framebuffer;

    if (IsInitCalled()) {
        InitObject(m_framebuffer);
    }
}

void Camera::SetTranslation(const Vector3 &translation)
{
    m_translation = translation;
    m_next_translation = translation;
    
    m_previous_view_matrix = m_view_mat;

    if (m_camera_controller != nullptr) {
        m_camera_controller->SetTranslation(translation);
    }

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetNextTranslation(const Vector3 &translation)
{
    m_next_translation = translation;

    if (m_camera_controller != nullptr) {
        m_camera_controller->SetNextTranslation(translation);
    }
}

void Camera::SetDirection(const Vector3 &direction)
{
    m_direction = direction;
    
    m_previous_view_matrix = m_view_mat;

    if (m_camera_controller != nullptr) {
        m_camera_controller->SetDirection(direction);
    }

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetUpVector(const Vector3 &up)
{
    m_up = up;
    
    m_previous_view_matrix = m_view_mat;

    if (m_camera_controller != nullptr) {
        m_camera_controller->SetUpVector(up);
    }

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::Rotate(const Vector3 &axis, float radians)
{
    m_direction.Rotate(axis, radians);
    m_direction.Normalize();
    
    m_previous_view_matrix = m_view_mat;

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetViewMatrix(const Matrix4 &view_mat)
{
    m_previous_view_matrix = m_view_mat;
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
    m_previous_view_matrix = m_view_mat;

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
    // [0, 1] -> [-1, 1]

    return {
        screen.x * 2.0f - 1.0f,//1.0f - (2.0f * screen.x),
        screen.y * 2.0f - 1.0f,//1.0f - (2.0f * screen.y),
        1.0f
    };
}

Vector4 Camera::TransformNDCToWorld(const Vector3 &ndc) const
{
    const Vector4 clip(ndc, 1.0f);

    Vector4 eye = m_proj_mat.Inverted() * clip;
    eye /= eye.w;
    // eye = Vector4(eye.x, eye.y, -1.0f, 0.0f);

    return m_view_mat.Inverted() * eye;
}

Vector3 Camera::TransformWorldToNDC(const Vector3 &world) const
{
    return m_view_proj_mat * world;
}

Vector2 Camera::TransformWorldToScreen(const Vector3 &world) const
{
    return TransformNDCToScreen(m_view_proj_mat * world);
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

Vector2 Camera::GetPixelSize() const
{
    return Vector2::one / Vector2 { Float(GetWidth()), Float(GetHeight()) };
}

void Camera::Update(GameCounter::TickUnit dt)
{
    if (m_camera_controller) {
        m_camera_controller->m_camera = this;

        m_camera_controller->UpdateCommandQueue(dt);
        m_camera_controller->UpdateLogic(dt);
    }

    m_translation = m_next_translation;

    UpdateMatrices();

    PUSH_RENDER_COMMAND(UpdateCameraDrawProxy, 
        this,
        CameraDrawProxy {
            .view = m_view_mat,
            .projection = m_proj_mat,
            .previous_view = m_previous_view_matrix,
            .position = m_translation,
            .direction = m_direction,
            .up = m_up,
            .dimensions = Extent2D { UInt(MathUtil::Abs(m_width)), UInt(MathUtil::Abs(m_height)) },
            .clip_near = m_near,
            .clip_far = m_far,
            .fov = m_fov,
            .frustum = m_frustum
        }
    );
}

void Camera::UpdateViewMatrix()
{
    if (m_camera_controller) {
        m_camera_controller->UpdateViewMatrix();
    }
}

void Camera::UpdateProjectionatrix()
{
    if (m_camera_controller) {
        m_camera_controller->UpdateProjectionMatrix();
    }
}

void Camera::UpdateMatrices()
{
    if (m_camera_controller) {
        m_camera_controller->UpdateViewMatrix();
        m_camera_controller->UpdateProjectionMatrix();
    }

    UpdateViewProjectionMatrix();
}

} // namespace hyperion::v2
