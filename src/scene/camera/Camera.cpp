/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/camera/Camera.hpp>
#include <scene/camera/streaming/CameraStreamingVolume.hpp>

#include <core/math/Halton.hpp>

#include <rendering/ShaderGlobals.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>

#include <core/object/HypClassUtils.hpp>

#include <system/AppContext.hpp>

#include <core/utilities/Result.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

class Camera;

#pragma region CameraController

CameraController::CameraController(CameraProjectionMode projection_mode)
    : m_input_handler(MakeRefCountedPtr<NullInputHandler>()),
      m_camera(nullptr),
      m_projection_mode(projection_mode),
      m_command_queue_count { 0 },
      m_mouse_lock_requested(false)
{
}

void CameraController::OnAdded(Camera* camera)
{
    HYP_SCOPE;

    m_camera = camera;

    OnAdded();
}

void CameraController::OnAdded()
{
    // Do nothing
}

void CameraController::OnRemoved()
{
    // Do nothing
}

void CameraController::OnActivated()
{
    // Do nothing
}

void CameraController::OnDeactivated()
{
    SetIsMouseLockRequested(false);
}

void CameraController::PushCommand(const CameraCommand& command)
{
    HYP_SCOPE;

    std::lock_guard guard(m_command_queue_mutex);

    ++m_command_queue_count;

    m_command_queue.Push(command);
}

void CameraController::UpdateCommandQueue(GameCounter::TickUnit dt)
{
    HYP_SCOPE;

    if (m_command_queue_count == 0)
    {
        return;
    }

    std::lock_guard guard(m_command_queue_mutex);

    while (m_command_queue.Any())
    {
        RespondToCommand(m_command_queue.Front(), dt);

        m_command_queue.Pop();
    }

    m_command_queue_count = 0;
}

void CameraController::SetIsMouseLockRequested(bool mouse_lock_requested)
{
    m_mouse_lock_requested = mouse_lock_requested;
}

#pragma endregion CameraController

#pragma region NullCameraController

NullCameraController::NullCameraController()
    : CameraController(CameraProjectionMode::NONE)
{
}

void NullCameraController::UpdateLogic(double dt)
{
}

void NullCameraController::UpdateViewMatrix()
{
}

void NullCameraController::UpdateProjectionMatrix()
{
}

#pragma endregion NullCameraController

#pragma region Camera

Camera::Camera()
    : Camera(128, 128)
{
}

Camera::Camera(int width, int height)
    : HypObject(),
      m_name(Name::Unique("Camera_")),
      m_flags(CameraFlags::NONE),
      m_match_window_size_ratio(1.0f),
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
      m_up(Vec3f::UnitY()),
      m_render_resource(nullptr)
{
    // make sure there is always at least 1 camera controller
    m_camera_controllers.PushBack(MakeRefCountedPtr<NullCameraController>());
}

Camera::Camera(float fov, int width, int height, float _near, float _far)
    : HypObject(),
      m_name(Name::Unique("Camera_")),
      m_flags(CameraFlags::NONE),
      m_match_window_size_ratio(1.0f),
      m_fov(fov),
      m_width(width),
      m_height(height),
      m_translation(Vec3f::Zero()),
      m_direction(Vec3f::UnitZ()),
      m_up(Vec3f::UnitY()),
      m_render_resource(nullptr)
{
    // make sure there is always at least 1 camera controller
    m_camera_controllers.PushBack(MakeRefCountedPtr<NullCameraController>());

    SetToPerspectiveProjection(fov, _near, _far);
}

Camera::Camera(int width, int height, float left, float right, float bottom, float top, float _near, float _far)
    : HypObject(),
      m_name(Name::Unique("Camera_")),
      m_flags(CameraFlags::NONE),
      m_match_window_size_ratio(1.0f),
      m_fov(0.0f),
      m_width(width),
      m_height(height),
      m_translation(Vec3f::Zero()),
      m_direction(Vec3f::UnitZ()),
      m_up(Vec3f::UnitY()),
      m_render_resource(nullptr)
{
    // make sure there is always at least 1 camera controller
    m_camera_controllers.PushBack(MakeRefCountedPtr<NullCameraController>());

    SetToOrthographicProjection(left, right, bottom, top, _near, _far);
}

Camera::~Camera()
{
    while (HasActiveCameraController())
    {
        const RC<CameraController> camera_controller = m_camera_controllers.PopBack();

        camera_controller->OnDeactivated();
        camera_controller->OnRemoved();
    }

    if (m_render_resource != nullptr)
    {
        m_render_resource->EnqueueUnbind();
        m_render_resource->DecRef();
        FreeResource(m_render_resource);
    }
}

void Camera::Init()
{
    if (IsInitCalled())
    {
        return;
    }

    HypObject::Init();

    m_streaming_volume = CreateObject<CameraStreamingVolume>();
    /// \todo: Set a proper bounding box for the streaming volume
    m_streaming_volume->SetBoundingBox(BoundingBox(m_translation - 10.0f, m_translation + 10.0f));
    InitObject(m_streaming_volume);

    if (m_flags & CameraFlags::MATCH_WINDOW_SIZE)
    {
        auto init_match_window_size = [this]() -> TResult<>
        {
            const RC<AppContextBase>& app_context = g_engine->GetAppContext();

            if (!app_context)
            {
                return HYP_MAKE_ERROR(Error, "No valid app context!");
            }

            if (!app_context->GetMainWindow())
            {
                return HYP_MAKE_ERROR(Error, "No main window set!");
            }

            const Vec2i window_size = MathUtil::Max(Vec2i(MathUtil::Round(Vec2f(app_context->GetMainWindow()->GetDimensions()) * m_match_window_size_ratio)), Vec2i::One());

            m_width = window_size.x;
            m_height = window_size.y;

            RemoveDelegateHandler(NAME("HandleWindowSizeChanged"));

            AddDelegateHandler(NAME("HandleWindowSizeChanged"), app_context->GetMainWindow()->OnWindowSizeChanged.Bind([this](Vec2i window_size)
                                                                    {
                                                                        HYP_NAMED_SCOPE("Update Camera size based on window size");

                                                                        Threads::AssertOnThread(g_game_thread);

                                                                        window_size = MathUtil::Max(Vec2i(MathUtil::Round(Vec2f(window_size) * m_match_window_size_ratio)), Vec2i::One());

                                                                        m_width = window_size.x;
                                                                        m_height = window_size.y;

                                                                        HYP_LOG(Camera, Debug, "Camera window size (change): {}", window_size);
                                                                    },
                                                                    /* require_current_thread */ true));

            HYP_LOG(Camera, Debug, "Camera window size: {}", window_size);

            return {};
        };

        if (auto match_window_size_result = init_match_window_size(); match_window_size_result.HasError())
        {
            HYP_LOG(Camera, Error, "Camera with MATCH_WINDOW_SIZE flag cannot match window size: {}", match_window_size_result.GetError().GetMessage());
        }
    }

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
        {
            if (m_render_resource != nullptr)
            {
                m_render_resource->DecRef();
                FreeResource(m_render_resource);

                m_render_resource = nullptr;
            }
        }));

    m_render_resource = AllocateResource<RenderCamera>(this);

    UpdateMatrices();

    m_render_resource->SetBufferData(CameraShaderData {
        .view = m_view_mat,
        .projection = m_proj_mat,
        .previous_view = m_previous_view_matrix,
        .dimensions = Vec4u { uint32(MathUtil::Abs(m_width)), uint32(MathUtil::Abs(m_height)), 0, 1 },
        .camera_position = Vec4f(m_translation, 1.0f),
        .camera_direction = Vec4f(m_direction, 1.0f),
        .camera_near = m_near,
        .camera_far = m_far,
        .camera_fov = m_fov,
        .id = GetID().Value() });

    m_render_resource->IncRef();

    SetReady(true);
}

void Camera::SetCameraControllers(const Array<RC<CameraController>>& camera_controllers)
{
    HYP_SCOPE;

    if (HasActiveCameraController())
    {
        if (const RC<CameraController>& current_camera_controller = GetCameraController())
        {
            current_camera_controller->OnDeactivated();
        }

        for (SizeType i = m_camera_controllers.Size(); i > 1; --i)
        {
            m_camera_controllers[i]->OnRemoved();
        }

        m_camera_controllers.Resize(1); // Keep the null camera controller
    }

    CameraController* active_camera_controller = nullptr;

    for (const RC<CameraController>& camera_controller : camera_controllers)
    {
        if (!camera_controller || camera_controller->IsInstanceOf<NullCameraController>())
        {
            continue;
        }

        camera_controller->OnAdded(this);

        m_camera_controllers.PushBack(camera_controller);

        active_camera_controller = camera_controller.Get();
    }

    if (active_camera_controller != nullptr)
    {
        active_camera_controller->OnActivated();

        UpdateMouseLocked();

        UpdateViewMatrix();
        UpdateProjectionMatrix();
        UpdateViewProjectionMatrix();
    }
}

void Camera::AddCameraController(const RC<CameraController>& camera_controller)
{
    HYP_SCOPE;

    if (!camera_controller || camera_controller->IsInstanceOf<NullCameraController>())
    {
        return;
    }

    if (m_camera_controllers.Contains(camera_controller))
    {
        return;
    }

    if (HasActiveCameraController())
    {
        if (const RC<CameraController>& current_camera_controller = GetCameraController())
        {
            current_camera_controller->OnDeactivated();
        }
    }

    m_camera_controllers.PushBack(camera_controller);

    camera_controller->OnAdded(this);
    camera_controller->OnActivated();

    UpdateMouseLocked();

    UpdateViewMatrix();
    UpdateProjectionMatrix();
    UpdateViewProjectionMatrix();
}

bool Camera::RemoveCameraController(const RC<CameraController>& camera_controller)
{
    HYP_SCOPE;

    if (!camera_controller || camera_controller->IsInstanceOf<NullCameraController>())
    {
        return false;
    }

    auto it = m_camera_controllers.Find(camera_controller);

    if (it == m_camera_controllers.End())
    {
        return false;
    }

    bool should_activate_camera_controller = false;

    if (camera_controller == GetCameraController())
    {
        camera_controller->OnDeactivated();

        should_activate_camera_controller = true;
    }

    camera_controller->OnRemoved();

    m_camera_controllers.Erase(it);

    if (should_activate_camera_controller && HasActiveCameraController())
    {
        if (const RC<CameraController>& current_camera_controller = GetCameraController())
        {
            current_camera_controller->OnActivated();
        }
    }

    UpdateMouseLocked();

    UpdateViewMatrix();
    UpdateProjectionMatrix();
    UpdateViewProjectionMatrix();

    return true;
}

void Camera::SetTranslation(const Vec3f& translation)
{
    HYP_SCOPE;

    m_translation = translation;
    m_next_translation = translation;

    m_previous_view_matrix = m_view_mat;

    if (HasActiveCameraController())
    {
        if (const RC<CameraController>& camera_controller = GetCameraController())
        {
            camera_controller->SetTranslation(translation);
        }
    }

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetNextTranslation(const Vec3f& translation)
{
    HYP_SCOPE;

    m_next_translation = translation;

    if (HasActiveCameraController())
    {
        if (const RC<CameraController>& camera_controller = GetCameraController())
        {
            camera_controller->SetNextTranslation(translation);
        }
    }
}

void Camera::SetDirection(const Vec3f& direction)
{
    HYP_SCOPE;

    m_direction = direction;

    if (HasActiveCameraController())
    {
        if (const RC<CameraController>& camera_controller = GetCameraController())
        {
            camera_controller->SetDirection(direction);
        }
    }

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetUpVector(const Vec3f& up)
{
    HYP_SCOPE;

    m_up = up;

    if (HasActiveCameraController())
    {
        if (const RC<CameraController>& camera_controller = GetCameraController())
        {
            camera_controller->SetUpVector(up);
        }
    }

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::Rotate(const Vec3f& axis, float radians)
{
    HYP_SCOPE;

    m_direction.Rotate(axis, radians);
    m_direction.Normalize();

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetViewMatrix(const Matrix4& view_mat)
{
    HYP_SCOPE;

    m_previous_view_matrix = m_view_mat;
    m_view_mat = view_mat;

    UpdateViewProjectionMatrix();
}

void Camera::SetProjectionMatrix(const Matrix4& proj_mat)
{
    HYP_SCOPE;

    m_proj_mat = proj_mat;

    UpdateViewProjectionMatrix();
}

void Camera::SetViewProjectionMatrix(const Matrix4& view_mat, const Matrix4& proj_mat)
{
    HYP_SCOPE;

    m_previous_view_matrix = m_view_mat;

    m_view_mat = view_mat;
    m_proj_mat = proj_mat;

    UpdateViewProjectionMatrix();
}

void Camera::UpdateViewProjectionMatrix()
{
    HYP_SCOPE;

    m_view_proj_mat = m_proj_mat * m_view_mat;

    m_frustum.SetFromViewProjectionMatrix(m_view_proj_mat);
}

Vec3f Camera::TransformScreenToNDC(const Vec2f& screen) const
{
    // [0, 1] -> [-1, 1]

    return {
        screen.x * 2.0f - 1.0f, // 1.0f - (2.0f * screen.x),
        screen.y * 2.0f - 1.0f, // 1.0f - (2.0f * screen.y),
        1.0f
    };
}

Vec4f Camera::TransformNDCToWorld(const Vec3f& ndc) const
{
    const Vec4f clip(ndc, 1.0f);

    Vec4f eye = m_proj_mat.Inverted() * clip;
    eye /= eye.w;
    // eye = Vec4f(eye.x, eye.y, -1.0f, 0.0f);

    return m_view_mat.Inverted() * eye;
}

Vec3f Camera::TransformWorldToNDC(const Vec3f& world) const
{
    return m_view_proj_mat * world;
}

Vec2f Camera::TransformWorldToScreen(const Vec3f& world) const
{
    return TransformNDCToScreen(m_view_proj_mat * world);
}

Vec2f Camera::TransformNDCToScreen(const Vec3f& ndc) const
{
    return {
        (0.5f * ndc.x) + 0.5f,
        (0.5f * ndc.y) + 0.5f
    };
}

Vec4f Camera::TransformScreenToWorld(const Vec2f& screen) const
{
    return TransformNDCToWorld(TransformScreenToNDC(screen));
}

Vec2f Camera::GetPixelSize() const
{
    return Vec2f::One() / Vec2f { float(GetWidth()), float(GetHeight()) };
}

void Camera::Update(GameCounter::TickUnit dt)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertReady();

    if (HasActiveCameraController())
    {
        if (const RC<CameraController>& camera_controller = GetCameraController())
        {
            UpdateMouseLocked();

            camera_controller->UpdateCommandQueue(dt);
            camera_controller->UpdateLogic(dt);
        }
    }

    m_translation = m_next_translation;

    UpdateMatrices();

    if (m_streaming_volume.IsValid())
    {
        /// \todo: Set a proper bounding box for the streaming volume
        m_streaming_volume->SetBoundingBox(BoundingBox(m_translation - 10.0f, m_translation + 10.0f));
    }

    m_render_resource->SetBufferData(CameraShaderData {
        .view = m_view_mat,
        .projection = m_proj_mat,
        .previous_view = m_previous_view_matrix,
        .dimensions = Vec4u { uint32(MathUtil::Abs(m_width)), uint32(MathUtil::Abs(m_height)), 0, 1 },
        .camera_position = Vec4f(m_translation, 1.0f),
        .camera_direction = Vec4f(m_translation, 1.0f),
        .camera_near = m_near,
        .camera_far = m_far,
        .camera_fov = m_fov,
        .id = GetID().Value() });
}

void Camera::UpdateViewMatrix()
{
    HYP_SCOPE;

    m_previous_view_matrix = m_view_mat;

    if (HasActiveCameraController())
    {
        if (const RC<CameraController>& camera_controller = GetCameraController())
        {
            camera_controller->UpdateViewMatrix();
        }
    }
}

void Camera::UpdateProjectionMatrix()
{
    HYP_SCOPE;

    if (HasActiveCameraController())
    {
        if (const RC<CameraController>& camera_controller = GetCameraController())
        {
            camera_controller->UpdateProjectionMatrix();
        }
    }
}

void Camera::UpdateMatrices()
{
    HYP_SCOPE;

    m_previous_view_matrix = m_view_mat;

    if (HasActiveCameraController())
    {
        if (const RC<CameraController>& camera_controller = GetCameraController())
        {
            camera_controller->UpdateViewMatrix();
            camera_controller->UpdateProjectionMatrix();
        }
    }

    UpdateViewProjectionMatrix();
}

void Camera::UpdateMouseLocked()
{
    HYP_SCOPE;

    bool should_lock_mouse = false;

    if (const RC<CameraController>& camera_controller = GetCameraController(); camera_controller && !camera_controller->IsInstanceOf<NullCameraController>())
    {
        if (camera_controller->IsMouseLockAllowed() && camera_controller->IsMouseLockRequested())
        {
            should_lock_mouse = true;
        }
    }

    if (should_lock_mouse)
    {
        if (!m_mouse_lock_scope)
        {
            if (const RC<AppContextBase>& app_context = g_engine->GetAppContext())
            {
                m_mouse_lock_scope = app_context->GetInputManager()->AcquireMouseLock();
            }
        }
    }
    else
    {
        m_mouse_lock_scope.Reset();
    }
}

#pragma endregion Camera

} // namespace hyperion
