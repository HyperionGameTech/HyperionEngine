/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/camera/Camera.hpp>
#include <scene/camera/streaming/CameraStreamingVolume.hpp>

#include <scene/World.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <streaming/StreamingManager.hpp>
#include <streaming/StreamingVolume.hpp>

#include <core/math/Halton.hpp>

#include <rendering/RenderFramebuffer.hpp>
#include <rendering/RenderProxy.hpp>

#include <core/object/HypClassUtils.hpp>

#include <system/AppContext.hpp>

#include <core/utilities/Result.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

class Camera;

static const Handle<NullInputHandler>& GetNullInputHandler()
{
    static const Handle<NullInputHandler> nullInputHandler = CreateObject<NullInputHandler>();
    return nullInputHandler;
}

static const Handle<NullCameraController>& GetNullCameraController()
{
    static const Handle<NullCameraController> nullCameraController = CreateObject<NullCameraController>();
    return nullCameraController;
}

#pragma region CameraController

CameraController::CameraController(CameraProjectionMode projectionMode)
    : m_inputHandler(GetNullInputHandler()),
      m_camera(nullptr),
      m_projectionMode(projectionMode),
      m_commandQueueCount { 0 },
      m_mouseLockRequested(false)
{
}

void CameraController::SetInputHandler(const Handle<InputHandlerBase>& inputHandler)
{
    if (m_inputHandler == inputHandler)
    {
        return;
    }

    m_inputHandler = inputHandler;

    if (IsInitCalled())
    {
        InitObject(m_inputHandler);
    }
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

    std::lock_guard guard(m_commandQueueMutex);

    ++m_commandQueueCount;

    m_commandQueue.Push(command);
}

void CameraController::UpdateCommandQueue(float dt)
{
    HYP_SCOPE;

    if (m_commandQueueCount == 0)
    {
        return;
    }

    std::lock_guard guard(m_commandQueueMutex);

    while (m_commandQueue.Any())
    {
        RespondToCommand(m_commandQueue.Front(), dt);

        m_commandQueue.Pop();
    }

    m_commandQueueCount = 0;
}

void CameraController::SetIsMouseLockRequested(bool mouseLockRequested)
{
    m_mouseLockRequested = mouseLockRequested;
}

#pragma endregion CameraController

#pragma region NullCameraController

NullCameraController::NullCameraController()
    : CameraController(CameraProjectionMode::NONE)
{
}

void NullCameraController::Init()
{
    SetReady(true);
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
    : Entity(),
      m_name(Name::Unique("Camera_")),
      m_flags(CameraFlags::NONE),
      m_matchWindowSizeRatio(1.0f),
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
    // make sure there is always at least 1 camera controller
    m_cameraControllers.PushBack(GetNullCameraController());

    m_entityInitInfo.receivesUpdate = true;
    m_entityInitInfo.canEverUpdate = true;
}

Camera::Camera(float fov, int width, int height, float _near, float _far)
    : Entity(),
      m_name(Name::Unique("Camera_")),
      m_flags(CameraFlags::NONE),
      m_matchWindowSizeRatio(1.0f),
      m_fov(fov),
      m_width(width),
      m_height(height),
      m_translation(Vec3f::Zero()),
      m_direction(Vec3f::UnitZ()),
      m_up(Vec3f::UnitY())
{
    // make sure there is always at least 1 camera controller
    m_cameraControllers.PushBack(GetNullCameraController());

    SetToPerspectiveProjection(fov, _near, _far);

    m_entityInitInfo.receivesUpdate = true;
    m_entityInitInfo.canEverUpdate = true;
}

Camera::Camera(int width, int height, float left, float right, float bottom, float top, float _near, float _far)
    : Entity(),
      m_name(Name::Unique("Camera_")),
      m_flags(CameraFlags::NONE),
      m_matchWindowSizeRatio(1.0f),
      m_fov(0.0f),
      m_width(width),
      m_height(height),
      m_translation(Vec3f::Zero()),
      m_direction(Vec3f::UnitZ()),
      m_up(Vec3f::UnitY())
{
    // make sure there is always at least 1 camera controller
    m_cameraControllers.PushBack(GetNullCameraController());

    SetToOrthographicProjection(left, right, bottom, top, _near, _far);

    m_entityInitInfo.receivesUpdate = true;
    m_entityInitInfo.canEverUpdate = true;
}

Camera::~Camera()
{
    while (HasActiveCameraController())
    {
        const Handle<CameraController> cameraController = m_cameraControllers.PopBack();

        cameraController->OnDeactivated();
        cameraController->OnRemoved();
    }
}

void Camera::Init()
{
    m_streamingVolume = CreateObject<CameraStreamingVolume>();
    /// \todo: Set a proper bounding box for the streaming volume
    m_streamingVolume->SetBoundingBox(BoundingBox(m_translation - 10.0f, m_translation + 10.0f));
    InitObject(m_streamingVolume);

    if (m_flags & CameraFlags::MATCH_WINDOW_SIZE)
    {
        auto initMatchWindowSize = [this]() -> TResult<>
        {
            const Handle<AppContextBase>& appContext = g_engineDriver->GetAppContext();

            if (!appContext)
            {
                return HYP_MAKE_ERROR(Error, "No valid app context!");
            }

            if (!appContext->GetMainWindow())
            {
                return HYP_MAKE_ERROR(Error, "No main window set!");
            }

            const Vec2i windowSize = MathUtil::Max(Vec2i(MathUtil::Round(Vec2f(appContext->GetMainWindow()->GetDimensions()) * m_matchWindowSizeRatio)), Vec2i::One());

            m_width = windowSize.x;
            m_height = windowSize.y;

            RemoveDelegateHandler(NAME("HandleWindowSizeChanged"));

            AddDelegateHandler(
                NAME("HandleWindowSizeChanged"),
                appContext->GetMainWindow()->OnWindowSizeChanged.BindThreaded([this](Vec2i windowSize)
                    {
                        HYP_NAMED_SCOPE("Update Camera size based on window size");

                        Threads::AssertOnThread(g_gameThread);

                        windowSize = MathUtil::Max(Vec2i(MathUtil::Round(Vec2f(windowSize) * m_matchWindowSizeRatio)), Vec2i::One());

                        m_width = windowSize.x;
                        m_height = windowSize.y;

                        HYP_LOG(Camera, Debug, "Camera window size (change): {}", windowSize);
                    },
                    g_gameThread));

            HYP_LOG(Camera, Debug, "Camera window size: {}", windowSize);

            return {};
        };

        if (auto matchWindowSizeResult = initMatchWindowSize(); matchWindowSizeResult.HasError())
        {
            HYP_LOG(Camera, Error, "Camera with MATCH_WINDOW_SIZE flag cannot match window size: {}", matchWindowSizeResult.GetError().GetMessage());
        }
    }

    for (const Handle<CameraController>& cameraController : m_cameraControllers)
    {
        InitObject(cameraController);
    }

    if (const Handle<CameraController>& cameraController = GetCameraController(); cameraController && !cameraController->IsA<NullCameraController>())
    {
        cameraController->OnAdded(this);
        cameraController->OnActivated();
    }

    UpdateMouseLocked();

    UpdateViewMatrix();
    UpdateProjectionMatrix();
    UpdateViewProjectionMatrix();

    SetReady(true);
}

void Camera::SetCameraControllers(const Array<Handle<CameraController>>& cameraControllers)
{
    HYP_SCOPE;

    if (HasActiveCameraController())
    {
        if (const Handle<CameraController>& currentCameraController = GetCameraController())
        {
            currentCameraController->OnDeactivated();
        }

        for (SizeType i = m_cameraControllers.Size(); i > 1; --i)
        {
            m_cameraControllers[i]->OnRemoved();
        }

        m_cameraControllers.Resize(1); // Keep the null camera controller
    }

    CameraController* activeCameraController = nullptr;

    for (const Handle<CameraController>& cameraController : cameraControllers)
    {
        if (!cameraController || cameraController->IsA<NullCameraController>())
        {
            continue;
        }

        cameraController->OnAdded(this);

        m_cameraControllers.PushBack(cameraController);

        activeCameraController = cameraController.Get();
    }

    if (activeCameraController != nullptr)
    {
        activeCameraController->OnActivated();

        UpdateMouseLocked();

        UpdateViewMatrix();
        UpdateProjectionMatrix();
        UpdateViewProjectionMatrix();
    }
}

void Camera::AddCameraController(const Handle<CameraController>& cameraController)
{
    HYP_SCOPE;

    if (!cameraController || cameraController->IsA<NullCameraController>())
    {
        return;
    }

    if (m_cameraControllers.Contains(cameraController))
    {
        return;
    }

    if (HasActiveCameraController())
    {
        if (const Handle<CameraController>& currentCameraController = GetCameraController())
        {
            currentCameraController->OnDeactivated();
        }
    }

    m_cameraControllers.PushBack(cameraController);

    if (IsInitCalled())
    {
        InitObject(cameraController);

        cameraController->OnAdded(this);
        cameraController->OnActivated();

        UpdateMouseLocked();

        UpdateViewMatrix();
        UpdateProjectionMatrix();
        UpdateViewProjectionMatrix();
    }
}

bool Camera::RemoveCameraController(const Handle<CameraController>& cameraController)
{
    HYP_SCOPE;

    if (!cameraController || cameraController->IsA<NullCameraController>())
    {
        return false;
    }

    auto it = m_cameraControllers.Find(cameraController);

    if (it == m_cameraControllers.End())
    {
        return false;
    }

    m_cameraControllers.Erase(it);

    if (IsInitCalled())
    {
        bool shouldActivateCameraController = false;

        if (cameraController == GetCameraController())
        {
            cameraController->OnDeactivated();

            shouldActivateCameraController = true;
        }

        cameraController->OnRemoved();

        if (shouldActivateCameraController && HasActiveCameraController())
        {
            if (const Handle<CameraController>& currentCameraController = GetCameraController())
            {
                currentCameraController->OnActivated();
            }
        }

        UpdateMouseLocked();

        UpdateViewMatrix();
        UpdateProjectionMatrix();
        UpdateViewProjectionMatrix();
    }

    return true;
}

void Camera::SetTranslation(const Vec3f& translation)
{
    HYP_SCOPE;

    m_translation = translation;
    m_nextTranslation = translation;

    m_previousViewMatrix = m_viewMat;

    if (HasActiveCameraController())
    {
        if (const Handle<CameraController>& cameraController = GetCameraController())
        {
            cameraController->SetTranslation(translation);
        }
    }

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetNextTranslation(const Vec3f& translation)
{
    HYP_SCOPE;

    m_nextTranslation = translation;

    if (HasActiveCameraController())
    {
        if (const Handle<CameraController>& cameraController = GetCameraController())
        {
            cameraController->SetNextTranslation(translation);
        }
    }
}

void Camera::SetDirection(const Vec3f& direction)
{
    HYP_SCOPE;

    m_direction = direction;

    if (HasActiveCameraController())
    {
        if (const Handle<CameraController>& cameraController = GetCameraController())
        {
            cameraController->SetDirection(direction);
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
        if (const Handle<CameraController>& cameraController = GetCameraController())
        {
            cameraController->SetUpVector(up);
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

void Camera::SetViewMatrix(const Matrix4& viewMat)
{
    HYP_SCOPE;

    m_previousViewMatrix = m_viewMat;
    m_viewMat = viewMat;

    UpdateViewProjectionMatrix();
}

void Camera::SetProjectionMatrix(const Matrix4& projMat)
{
    HYP_SCOPE;

    m_projMat = projMat;

    UpdateViewProjectionMatrix();
}

void Camera::SetViewProjectionMatrix(const Matrix4& viewMat, const Matrix4& projMat)
{
    HYP_SCOPE;

    m_previousViewMatrix = m_viewMat;

    m_viewMat = viewMat;
    m_projMat = projMat;

    UpdateViewProjectionMatrix();
}

void Camera::UpdateViewProjectionMatrix()
{
    HYP_SCOPE;

    m_viewProjMat = m_projMat * m_viewMat;

    m_frustum.SetFromViewProjectionMatrix(m_viewProjMat);

    SetNeedsRenderProxyUpdate();
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

    Vec4f eye = m_projMat.Inverted() * clip;
    eye /= eye.w;
    // eye = Vec4f(eye.x, eye.y, -1.0f, 0.0f);

    return m_viewMat.Inverted() * eye;
}

Vec3f Camera::TransformWorldToNDC(const Vec3f& world) const
{
    return m_viewProjMat * world;
}

Vec2f Camera::TransformWorldToScreen(const Vec3f& world) const
{
    return TransformNDCToScreen(m_viewProjMat * world);
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

void Camera::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    if (HasActiveCameraController())
    {
        if (const Handle<CameraController>& cameraController = GetCameraController())
        {
            UpdateMouseLocked();

            cameraController->UpdateCommandQueue(delta);
            cameraController->UpdateLogic(delta);
        }
    }

    m_translation = m_nextTranslation;

    UpdateMatrices();

    if (m_streamingVolume.IsValid())
    {
        /// \todo: Set a proper bounding box for the streaming volume
        m_streamingVolume->SetBoundingBox(BoundingBox(m_translation - 10.0f, m_translation + 10.0f));
    }

    SetNeedsRenderProxyUpdate();
}

void Camera::UpdateViewMatrix()
{
    HYP_SCOPE;

    m_previousViewMatrix = m_viewMat;

    if (HasActiveCameraController())
    {
        if (const Handle<CameraController>& cameraController = GetCameraController())
        {
            cameraController->UpdateViewMatrix();
        }
    }
}

void Camera::UpdateProjectionMatrix()
{
    HYP_SCOPE;

    if (HasActiveCameraController())
    {
        if (const Handle<CameraController>& cameraController = GetCameraController())
        {
            cameraController->UpdateProjectionMatrix();
        }
    }
}

void Camera::UpdateMatrices()
{
    HYP_SCOPE;

    m_previousViewMatrix = m_viewMat;

    if (HasActiveCameraController())
    {
        if (const Handle<CameraController>& cameraController = GetCameraController())
        {
            cameraController->UpdateViewMatrix();
            cameraController->UpdateProjectionMatrix();
        }
    }

    UpdateViewProjectionMatrix();
}

void Camera::UpdateMouseLocked()
{
    HYP_SCOPE;

    bool shouldLockMouse = false;

    if (const Handle<CameraController>& cameraController = GetCameraController(); cameraController && !cameraController->IsA<NullCameraController>())
    {
        if (cameraController->IsMouseLockAllowed() && cameraController->IsMouseLockRequested())
        {
            shouldLockMouse = true;
        }
    }

    if (shouldLockMouse)
    {
        if (!m_mouseLockScope)
        {
            if (const Handle<AppContextBase>& appContext = g_engineDriver->GetAppContext())
            {
                m_mouseLockScope = appContext->GetInputManager()->AcquireMouseLock();

                return;
            }
        }
    }
    else
    {
        m_mouseLockScope.Reset();
    }
}

void Camera::OnAddedToWorld(World* world)
{
    if (const Handle<WorldGrid>& worldGrid = world->GetWorldGrid())
    {
        AssertDebug(GetStreamingVolume().IsValid());

        worldGrid->GetStreamingManager()->AddStreamingVolume(GetStreamingVolume());
    }

    Entity::OnAddedToWorld(world);
}

void Camera::OnRemovedFromWorld(World* world)
{
    if (const Handle<WorldGrid>& worldGrid = world->GetWorldGrid())
    {
        worldGrid->GetStreamingManager()->RemoveStreamingVolume(GetStreamingVolume());
    }

    Entity::OnRemovedFromWorld(world);
}

void Camera::UpdateRenderProxy(RenderProxyCamera* proxy)
{
    proxy->camera = WeakHandleFromThis();

    proxy->viewFrustum = m_frustum;

    CameraShaderData& bufferData = proxy->bufferData;
    bufferData.id = Id().Value();
    bufferData.view = m_viewMat;
    bufferData.projection = m_projMat;
    bufferData.previousView = m_previousViewMatrix;
    bufferData.dimensions = Vec4u { uint32(MathUtil::Abs(m_width)), uint32(MathUtil::Abs(m_height)), 0, 1 };
    bufferData.cameraPosition = Vec4f(m_translation, 1.0f);
    bufferData.cameraDirection = Vec4f(m_direction, 1.0f);
    bufferData.cameraNear = m_near;
    bufferData.cameraFar = m_far;
    bufferData.cameraFov = m_fov;
}

#pragma endregion Camera

} // namespace hyperion
