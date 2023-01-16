#include "Camera.hpp"
#include <math/Halton.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Result;

class Camera;

struct RENDER_COMMAND(UpdateCameraDrawProxy) : RenderCommand
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

        Matrix4 offset_matrix;
        Vector2 jitter;
        Vector2 previous_jitter;

        // TODO: This probably won't work -- possibly,
        // we should set the jitter right at render time...

        const UInt frame_counter = Engine::Get()->GetRenderState().GetScene().scene.frame_counter;

        if (Engine::Get()->GetConfig().Get(CONFIG_TEMPORAL_AA)) {
            // TODO: Is this the main camera in the scene?
            if (draw_proxy.projection[3][3] < MathUtil::epsilon<Float>) {
                // perspective if [3][3] is zero
                static const HaltonSequence halton;

                const UInt halton_index = frame_counter % HaltonSequence::size;

                jitter = halton.sequence[halton_index];

                if (frame_counter != 0) {
                    previous_jitter = halton.sequence[(frame_counter - 1) % HaltonSequence::size];
                }

                const Vector2 pixel_size = Vector2::one / Vector2(draw_proxy.dimensions);

                jitter = (jitter * 2.0f - 1.0f) * pixel_size * 0.5f;
                previous_jitter = (previous_jitter * 2.0f - 1.0f) * pixel_size * 0.5f;

                offset_matrix[0][3] += jitter.x;
                offset_matrix[1][3] += jitter.y;
            }
        }

        CameraShaderData shader_data { };

        shader_data.view             = draw_proxy.view;
        shader_data.projection       = offset_matrix * draw_proxy.projection;
        shader_data.previous_view    = draw_proxy.previous_view;
        shader_data.dimensions       = { draw_proxy.dimensions.width, draw_proxy.dimensions.height, 0, 0 };
        shader_data.camera_position  = draw_proxy.position.ToVector4();
        shader_data.camera_direction = draw_proxy.direction.ToVector4();
        shader_data.camera_near      = draw_proxy.clip_near;
        shader_data.camera_fov       = draw_proxy.fov;
        shader_data.camera_far       = draw_proxy.clip_far;

        Engine::Get()->GetRenderData()->cameras.Set(camera->GetID().ToIndex(), shader_data);
        
        HYPERION_RETURN_OK;
    }
};

static Matrix4 BuildJitterMatrix(const Camera &camera, UInt frame_counter)
{
    if (camera.GetWidth() == 0 || camera.GetHeight() == 0) {
        return Matrix4::identity;
    }

    static const HaltonSequence halton;

    const Vector2 pixel_size = Vector2::one / Vector2(camera.GetWidth(), camera.GetHeight());
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
    : EngineComponentBase(),
      HasDrawProxy(),
      m_fov(60.0f),
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
    : EngineComponentBase(),
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
    : EngineComponentBase(),
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

    EngineComponentBase::Init();

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
    return Vector2::one / Vector((float)GetWidth(), (float)GetHeight());
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

    // TODO: Check that matrices have changed before this.
    Engine::Get()->GetWorld()->GetOctree().CalculateVisibility(this);

    RenderCommands::Push<RENDER_COMMAND(UpdateCameraDrawProxy)>(
        this,
        CameraDrawProxy {
            .view = m_view_mat,
            .projection = m_proj_mat,
            .previous_view = m_previous_view_matrix,
            .position = m_translation,
            .direction = m_direction,
            .up = m_up,
            .dimensions = Extent2D { UInt(m_width), UInt(m_height) },
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
