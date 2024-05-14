/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/camera/Camera.hpp>

#include <math/Halton.hpp>

#include <rendering/ShaderGlobals.hpp>

#include <core/system/AppContext.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::Result;

class Camera;

#pragma region Render commands

struct RENDER_COMMAND(UpdateCameraDrawProxy) : renderer::RenderCommand
{
    Camera          *camera;
    CameraDrawProxy draw_proxy;

    RENDER_COMMAND(UpdateCameraDrawProxy)(Camera *camera, const CameraDrawProxy &draw_proxy)
        : camera(camera),
          draw_proxy(draw_proxy)
    {
    }

    virtual ~RENDER_COMMAND(UpdateCameraDrawProxy)() override = default;

    virtual Result operator()() override
    {
        camera->m_draw_proxy = draw_proxy;

        g_engine->GetRenderData()->cameras.Set(camera->GetID().ToIndex(), CameraShaderData {
            .view               = draw_proxy.view,
            .projection         = draw_proxy.projection,
            .previous_view      = draw_proxy.previous_view,
            .dimensions         = { draw_proxy.dimensions.width, draw_proxy.dimensions.height, 0, 1 },
            .camera_position    = Vec4f(draw_proxy.position, 1.0f),
            .camera_direction   = Vec4f(draw_proxy.position, 1.0f),
            .camera_near        = draw_proxy.clip_near,
            .camera_far         = draw_proxy.clip_far,
            .camera_fov         = draw_proxy.fov
        });
        
        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

static Matrix4 BuildJitterMatrix(const Camera &camera, uint frame_counter)
{
    if (camera.GetWidth() == 0 || camera.GetHeight() == 0) {
        return Matrix4::identity;
    }

    static const HaltonSequence halton;

    const Vec2f pixel_size = Vec2f::One() / Vec2f(float(MathUtil::Abs(camera.GetWidth())), float(MathUtil::Abs(camera.GetHeight())));
    const uint index = frame_counter % HaltonSequence::size;

    const Vec2f jitter = halton.sequence[index] * 2.0f - 1.0f;

    Matrix4 jitter_matrix = camera.GetProjectionMatrix();
    jitter_matrix[0][2] += jitter.x * pixel_size.x;
    jitter_matrix[1][2] += jitter.y * pixel_size.y;

    return jitter_matrix;
}

#pragma region CameraController

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

void CameraController::SetMouseLocked(bool mouse_locked)
{
    if (!g_engine->GetAppContext()) {
        return;
    }

    if (!g_engine->GetAppContext()->GetCurrentWindow()) {
        return;
    }

    g_engine->GetAppContext()->GetCurrentWindow()->SetMouseLocked(mouse_locked);
}

#pragma endregion CameraController

#pragma region Camera

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
      m_translation(Vec3f::Zero()),
      m_direction(Vec3f::UnitZ()),
      m_up(Vec3f::UnitY())
{
}

Camera::Camera(float fov, int width, int height, float _near, float _far)
    : BasicObject(),
      HasDrawProxy(),
      m_fov(fov),
      m_width(width),
      m_height(height),
      m_translation(Vec3f::Zero()),
      m_direction(Vec3f::UnitZ()),
      m_up(Vec3f::UnitY())
{
    SetToPerspectiveProjection(fov, _near, _far);
}

Camera::Camera(int width, int height, float left, float right, float bottom, float top, float _near, float _far)
    : BasicObject(),
      HasDrawProxy(),
      m_fov(0.0f),
      m_width(width),
      m_height(height),
      m_translation(Vec3f::Zero()),
      m_direction(Vec3f::UnitZ()),
      m_up(Vec3f::UnitY())
{
    SetToOrthographicProjection(left, right, bottom, top, _near, _far);
}

Camera::~Camera()
{
    SafeRelease(std::move(m_framebuffer));

    // Sync render commands to prevent dangling pointers to this
    HYP_SYNC_RENDER();
}

void Camera::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
    {
        SafeRelease(std::move(m_framebuffer));
    }));

    m_draw_proxy = CameraDrawProxy {
        .view           = m_view_mat,
        .projection     = m_proj_mat,
        .previous_view  = m_previous_view_matrix,
        .position       = m_translation,
        .direction      = m_direction,
        .up             = m_up,
        .dimensions     = Extent2D { uint(MathUtil::Abs(m_width)), uint(MathUtil::Abs(m_height)) },
        .clip_near      = m_near,
        .clip_far       = m_far,
        .fov            = m_fov,
        .frustum        = m_frustum
    };

    DeferCreate(m_framebuffer, g_engine->GetGPUDevice());

    SetReady(true);
}

void Camera::SetFramebuffer(const FramebufferRef &framebuffer)
{
    m_framebuffer = framebuffer;
}

void Camera::SetTranslation(const Vec3f &translation)
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

void Camera::SetNextTranslation(const Vec3f &translation)
{
    m_next_translation = translation;

    if (m_camera_controller != nullptr) {
        m_camera_controller->SetNextTranslation(translation);
    }
}

void Camera::SetDirection(const Vec3f &direction)
{
    m_direction = direction;

    if (m_camera_controller != nullptr) {
        m_camera_controller->SetDirection(direction);
    }

    // UpdateViewMatrix();
    // UpdateViewProjectionMatrix();
}

void Camera::SetUpVector(const Vec3f &up)
{
    m_up = up;

    if (m_camera_controller != nullptr) {
        m_camera_controller->SetUpVector(up);
    }

    // UpdateViewMatrix();
    // UpdateViewProjectionMatrix();
}

void Camera::Rotate(const Vec3f &axis, float radians)
{
    m_direction.Rotate(axis, radians);
    m_direction.Normalize();
    
    // m_previous_view_matrix = m_view_mat;

    // UpdateViewMatrix();
    // UpdateViewProjectionMatrix();
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

Vec3f Camera::TransformScreenToNDC(const Vec2f &screen) const
{
    // [0, 1] -> [-1, 1]

    return {
        screen.x * 2.0f - 1.0f,//1.0f - (2.0f * screen.x),
        screen.y * 2.0f - 1.0f,//1.0f - (2.0f * screen.y),
        1.0f
    };
}

Vec4f Camera::TransformNDCToWorld(const Vec3f &ndc) const
{
    const Vec4f clip(ndc, 1.0f);

    Vec4f eye = m_proj_mat.Inverted() * clip;
    eye /= eye.w;
    // eye = Vec4f(eye.x, eye.y, -1.0f, 0.0f);

    return m_view_mat.Inverted() * eye;
}

Vec3f Camera::TransformWorldToNDC(const Vec3f &world) const
{
    return m_view_proj_mat * world;
}

Vec2f Camera::TransformWorldToScreen(const Vec3f &world) const
{
    return TransformNDCToScreen(m_view_proj_mat * world);
}

Vec2f Camera::TransformNDCToScreen(const Vec3f &ndc) const
{
    return {
        (0.5f * ndc.x) + 0.5f,
        (0.5f * ndc.y) + 0.5f
    };
}

Vec4f Camera::TransformScreenToWorld(const Vec2f &screen) const
{
    return TransformNDCToWorld(TransformScreenToNDC(screen));
}

Vec2f Camera::GetPixelSize() const
{
    return Vec2f::One() / Vec2f { float(GetWidth()), float(GetHeight()) };
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
            .view           = m_view_mat,
            .projection     = m_proj_mat,
            .previous_view  = m_previous_view_matrix,
            .position       = m_translation,
            .direction      = m_direction,
            .up             = m_up,
            .dimensions     = Extent2D { uint(MathUtil::Abs(m_width)), uint(MathUtil::Abs(m_height)) },
            .clip_near      = m_near,
            .clip_far       = m_far,
            .fov            = m_fov,
            .frustum        = m_frustum
        }
    );
}

void Camera::UpdateViewMatrix()
{
    m_previous_view_matrix = m_view_mat;

    if (m_camera_controller) {
        m_camera_controller->UpdateViewMatrix();
    }
}

void Camera::UpdateProjectionMatrix()
{
    if (m_camera_controller) {
        m_camera_controller->UpdateProjectionMatrix();
    }
}

void Camera::UpdateMatrices()
{
    m_previous_view_matrix = m_view_mat;

    if (m_camera_controller) {
        m_camera_controller->UpdateViewMatrix();
        m_camera_controller->UpdateProjectionMatrix();
    }

    UpdateViewProjectionMatrix();
}

#pragma endregion Camera

} // namespace hyperion
