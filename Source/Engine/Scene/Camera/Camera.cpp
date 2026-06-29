/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Camera/Streaming/CameraStreamingVolume.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/WorldGrid/WorldGrid.hpp>

#include <Streaming/StreamingManager.hpp>
#include <Streaming/StreamingVolume.hpp>

#include <Rendering/Framebuffer.hpp>
#include <Rendering/RenderProxy.hpp>

#include <System/AppContext.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Halton.hpp>

#include <Core/Utilities/Result.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Input/InputManager.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineDriver.hpp>
#include <Framework/CVarManager.hpp>

#include <Camera.generated.inl>

namespace Hyperion {

static constexpr float CameraJitterScale = 0.25f;

extern CVar<bool> g_cvTAA;

static NullInputHandler* GetNullInputHandler()
{
    static struct NullInputHandlerInitializer
    {
        NullInputHandler* inputHandler;

        NullInputHandlerInitializer()
        {
            inputHandler = new NullInputHandler;

            g_engineDriver->GetDelegates().OnShutdown.Bind([this]()
                                                           {
                                                               if (inputHandler != nullptr)
                                                               {
                                                                   inputHandler->Release();
                                                                   inputHandler = nullptr;
                                                               }
                                                           })
                .Detach();
        }
    } s_initializer;

    return s_initializer.inputHandler;
}

static NullCameraController* GetNullCameraController()
{
    static struct NullCameraControllerInitializer
    {
        NullCameraController* controller;

        NullCameraControllerInitializer()
        {
            controller = new NullCameraController;

            g_engineDriver->GetDelegates().OnShutdown.Bind([this]()
                                                           {
                                                               if (controller != nullptr)
                                                               {
                                                                   controller->Release();
                                                                   controller = nullptr;
                                                               }
                                                           })
                .Detach();
        }
    } s_initializer;

    return s_initializer.controller;
}

#pragma region CameraController

CameraController::CameraController()
    : CameraController(CameraProjectionMode::NONE)
{
}

CameraController::CameraController(CameraProjectionMode projectionMode)
    : m_inputHandler(MakeStrongRef(GetNullInputHandler())),
      m_camera(nullptr),
      m_projectionMode(projectionMode),
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
    InitObject(m_inputHandler);
}

void CameraController::OnAdded(Camera* camera)
{
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

void NullCameraController::UpdateLogic(double delta)
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
      m_cameraFlags(CameraFlags::NONE),
      m_matchWindowSizeRatio(1.0f),
      m_direction(Vec3f::UnitZ()),
      m_up(Vec3f::UnitY()),
      m_width(width),
      m_height(height),
      m_near(0.01f),
      m_far(1000.0f),
      m_fov(50.0f),
      m_streamingVolumeAdded(false)
{
    // make sure there is always at least 1 camera controller
    m_cameraControllers.PushBack(MakeStrongRef(GetNullCameraController()));

    m_entityInitInfo.receivesUpdate = true;
    m_entityInitInfo.canEverUpdate = true;
}

Camera::Camera(float fov, int width, int height, float _near, float _far)
    : Entity(),
      m_cameraFlags(CameraFlags::NONE),
      m_matchWindowSizeRatio(1.0f),
      m_direction(Vec3f::UnitZ()),
      m_up(Vec3f::UnitY()),
      m_width(width),
      m_height(height),
      m_fov(fov)
{
    // make sure there is always at least 1 camera controller
    m_cameraControllers.PushBack(MakeStrongRef(GetNullCameraController()));

    SetToPerspectiveProjection(fov, _near, _far);

    m_entityInitInfo.receivesUpdate = true;
    m_entityInitInfo.canEverUpdate = true;
}

Camera::Camera(int width, int height, float left, float right, float bottom, float top, float _near, float _far)
    : Entity(),
      m_cameraFlags(CameraFlags::NONE),
      m_matchWindowSizeRatio(1.0f),
      m_direction(Vec3f::UnitZ()),
      m_up(Vec3f::UnitY()),
      m_width(width),
      m_height(height),
      m_fov(0.0f)
{
    // make sure there is always at least 1 camera controller
    m_cameraControllers.PushBack(MakeStrongRef(GetNullCameraController()));

    SetToOrthographicProjection(left, right, bottom, top, _near, _far);

    m_entityInitInfo.receivesUpdate = true;
    m_entityInitInfo.canEverUpdate = true;
}

Camera::~Camera()
{
    m_onWindowResizedHandle.Reset();
    m_onMainWindowChangedHandle.Reset();

    while (HasActiveCameraController())
    {
        const Handle<CameraController> cameraController = m_cameraControllers.PopBack();

        cameraController->OnDeactivated();
        cameraController->OnRemoved();
    }
}

void Camera::Init()
{
    Entity::Init();

    SetNodeFlags(m_nodeFlags | NodeFlags::ExcludeFromParentBounds | NodeFlags::ExcludeFromOctree);

    const Vec3f translation = GetWorldTranslation();

    UpdateStreamingVolume();
    UpdateMatchWindowSize();

    UpdateMouseLocked();

    UpdateViewMatrix();
    UpdateProjectionMatrix();
    UpdateViewProjectionMatrix();

    SetReady(true);
}

void Camera::SetCameraFlags(EnumFlags<CameraFlags> flags)
{
    if (flags == m_cameraFlags)
    {
        return;
    }

    const EnumFlags<CameraFlags> flagsBefore = m_cameraFlags;
    const EnumFlags<CameraFlags> changedFlags = flags ^ flagsBefore;

    m_cameraFlags = flags;

    if (changedFlags & CameraFlags::MatchWindowSize)
    {
        UpdateMatchWindowSize();
    }

    if (changedFlags & CameraFlags::HasStreamingVolume)
    {
        UpdateStreamingVolume();
    }

    MarkDirty();
}

void Camera::SetCameraControllers(const Array<Handle<CameraController>>& cameraControllers)
{
    if (HasActiveCameraController())
    {
        if (const Handle<CameraController>& currentCameraController = GetCameraController())
        {
            currentCameraController->OnDeactivated();
        }

        for (size_t i = m_cameraControllers.Size(); i > 1; --i)
        {
            m_cameraControllers[i - 1]->OnRemoved();
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

void Camera::AddCameraController(const Handle<CameraController>& cameraController, int index)
{
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

    size_t realIndex = 0;

    if (index < 0 || index >= int(m_cameraControllers.Size()))
    {
        m_cameraControllers.PushBack(cameraController);
        realIndex = m_cameraControllers.Size() - 1;
    }
    else
    {
        if (index == 0)
        {
            // cannot insert before null camera controller
            realIndex = 1;
        }
        else
        {
            realIndex = size_t(index);
        }

        m_cameraControllers.Insert(m_cameraControllers.Begin() + realIndex, cameraController);
    }

    InitObject(cameraController);

    cameraController->OnAdded(this);

    if (realIndex == m_cameraControllers.Size() - 1)
    {
        // newly added camera controller is the active one
        cameraController->OnActivated();
    }

    UpdateMouseLocked();

    UpdateViewMatrix();
    UpdateProjectionMatrix();
    UpdateViewProjectionMatrix();
}

bool Camera::RemoveCameraController(const Handle<CameraController>& cameraController)
{
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

void Camera::OnTransformUpdated()
{
    Entity::OnTransformUpdated();

    const Vec3f translation = GetWorldTranslation();

    m_nextTranslation = translation;

    if (HasActiveCameraController())
    {
        if (const Handle<CameraController>& cameraController = GetCameraController())
        {
            cameraController->SetTranslation(translation);
        }
    }

    UpdateMatrices();
}

void Camera::SetNextTranslation(const Vec3f& translation)
{
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
    m_direction = direction;

    if (HasActiveCameraController())
    {
        if (const Handle<CameraController>& cameraController = GetCameraController())
        {
            cameraController->SetDirection(direction);
        }
    }

    UpdateMatrices();
}

void Camera::SetUpVector(const Vec3f& up)
{
    m_up = up;

    if (HasActiveCameraController())
    {
        if (const Handle<CameraController>& cameraController = GetCameraController())
        {
            cameraController->SetUpVector(up);
        }
    }

    UpdateMatrices();
}

void Camera::Rotate(const Vec3f& axis, float radians)
{
    m_direction.Rotate(axis, radians);
    m_direction.Normalize();

    UpdateMatrices();
}

void Camera::SetViewMatrix(const Mat4f& viewMat)
{
    m_viewMat = viewMat;

    UpdateViewProjectionMatrix();
}

void Camera::SetProjectionMatrix(const Mat4f& projMat)
{
    m_projMat = projMat;

    UpdateViewProjectionMatrix();
}

void Camera::SetViewProjectionMatrix(const Mat4f& viewMat, const Mat4f& projMat)
{
    m_viewMat = viewMat;
    m_projMat = projMat;

    UpdateViewProjectionMatrix();
}

void Camera::UpdateViewProjectionMatrix()
{
    m_viewProjMat = m_projMat * m_viewMat;

    m_frustum.SetFromViewProjectionMatrix(m_viewProjMat);

    SetNeedsRenderProxyUpdate();
}

void Camera::UpdateJitter()
{
    if (m_width > 0 && m_height > 0 && MathUtil::ApproxEqual(m_projMat[3][3], 0.0f))
    {
        Mat4f::Jitter(m_jitterFrameCounter++, uint32(MathUtil::Abs(m_width)), uint32(MathUtil::Abs(m_height)), m_jitter);
        m_jitter *= CameraJitterScale;
    }
}

Vec3f Camera::TransformScreenToNDC(const Vec2f& screen) const
{
    // [0, 1] -> [-1, 1]

    return {
        screen.x * 2.0f - 1.0f,
        1.0f - (2.0f * screen.y),
        1.0f
    };
}

Vec4f Camera::TransformNDCToWorld(const Vec3f& ndc) const
{
    const Vec4f clip(ndc, 1.0f);

    Vec4f eye = m_projMat.Inverse().TransformVector(clip);
    eye /= eye.w;

    return m_viewMat.Inverse().TransformVector(eye);
}

Vec3f Camera::TransformWorldToNDC(const Vec3f& world) const
{
    return m_viewProjMat.TransformVector(world);
}

Vec2f Camera::TransformWorldToScreen(const Vec3f& world) const
{
    return TransformNDCToScreen(m_viewProjMat.TransformVector(world));
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
    return Vec2f::One() / Vec2f(GetDimensions());
}

void Camera::UpdateMatchWindowSize()
{
    if (m_cameraFlags & CameraFlags::MatchWindowSize)
    {
        if (m_onMainWindowChangedHandle.IsValid() && m_onWindowResizedHandle.IsValid())
        {
            return;
        }

        const auto HandleWindowChanged = [this](ApplicationWindow* window)
        {
            m_onWindowResizedHandle.Reset();

            if (window == nullptr)
            {
                return;
            }

            auto MatchWindowSize = [this, weakWindow = MakeWeakRef(window)](Vec2i windowSize)
            {
                Handle<ApplicationWindow> strongWindow = weakWindow.Lock();

                const float renderTargetScale = strongWindow.IsValid()
                    ? strongWindow->GetRenderTargetScale()
                    : 1.0f;

                Vec2i renderSize = Vec2i(Vec2f(windowSize) * renderTargetScale);
                renderSize = MathUtil::Max(Vec2i(MathUtil::Round(Vec2f(renderSize) * m_matchWindowSizeRatio)), Vec2i::One());

                SetDimensions(renderSize);
            };

            MatchWindowSize(window->GetSize());

            m_onWindowResizedHandle = window->OnWindowSizeChanged.BindThreaded(window, MatchWindowSize, g_simThread);
        };

        HandleWindowChanged(g_appContext->GetMainWindow());

        m_onMainWindowChangedHandle = g_appContext->OnCurrentWindowChanged.BindThreaded(g_appContext, HandleWindowChanged, g_simThread);
    }
    else
    {
        m_onWindowResizedHandle.Reset();
        m_onMainWindowChangedHandle.Reset();
    }
}

void Camera::UpdateStreamingVolume()
{
    if (m_cameraFlags & CameraFlags::HasStreamingVolume)
    {
        if (!m_streamingVolume.IsValid())
        {
            const Vec3f worldTranslation = GetWorldTranslation();

            m_streamingVolume = MakeHandle<CameraStreamingVolume>();
            m_streamingVolume->SetBoundingBox(BoundingBox(worldTranslation - 10.0f, worldTranslation + 10.0f));
            InitObject(m_streamingVolume);
        }

        if (!m_streamingVolumeAdded)
        {
            g_streamingManager->AddStreamingVolume(m_streamingVolume);
            m_streamingVolumeAdded = true;
        }
    }
    else
    {
        if (m_streamingVolumeAdded)
        {
            m_streamingVolumeAdded = false;
            g_streamingManager->RemoveStreamingVolume(m_streamingVolume);
        }
    }
}

void Camera::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread | ThreadCategory::THREAD_CATEGORY_TASK);

    if (HasActiveCameraController())
    {
        if (const Handle<CameraController>& cameraController = GetCameraController())
        {
            UpdateMouseLocked();

            cameraController->UpdateLogic(delta);
        }
    }

    if (m_nextTranslation != GetWorldTranslation())
    {
        SetWorldTranslation(m_nextTranslation);
    }
    else
    {
        UpdateMatrices();
    }

    if (g_cvTAA.Get())
    {
        UpdateJitter();
    }
    else
    {
        m_jitter = Vec4f::Zero();
    }

    if (m_streamingVolume.IsValid())
    {
        const Vec3f translation = GetWorldTranslation();

        /// \todo: Set a proper bounding box for the streaming volume
        m_streamingVolume->SetBoundingBox(BoundingBox(translation - 10.0f, translation + 10.0f));
    }

    SetNeedsRenderProxyUpdate();
}

void Camera::UpdateViewMatrix()
{
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
    bool shouldLockMouse = false;

    if (const Handle<CameraController>& cameraController = GetCameraController(); cameraController && !cameraController->IsA<NullCameraController>())
    {
        if (cameraController->IsMouseLockAllowed() && cameraController->IsMouseLockRequested())
        {
            shouldLockMouse = true;
        }
    }

    if (shouldLockMouse && IsInitCalled())
    {
        if (!m_mouseLockScope)
        {
            m_mouseLockScope = g_appContext->GetMainWindow()->GetInputManager()->AcquireMouseLock();

            return;
        }
    }
    else
    {
        m_mouseLockScope.Reset();
    }
}

void Camera::OnAddedToWorld(World* world)
{
    UpdateStreamingVolume();

    Entity::OnAddedToWorld(world);
}

void Camera::OnRemovedFromWorld(World* world)
{
    UpdateStreamingVolume();

    Entity::OnRemovedFromWorld(world);
}

void Camera::UpdateRenderProxy(RenderProxyCamera* proxy)
{
    proxy->camera = WeakHandleFromThis();

    proxy->viewFrustum = m_frustum;

    CameraShaderData& bufferData = proxy->bufferData;
    bufferData.viewMat = m_viewMat;
    bufferData.projMat = m_projMat;

    bufferData.inverseViewMat = m_viewMat.Inverse();
    bufferData.inverseProjMat = m_projMat.Inverse();

    bufferData.viewProjMat = m_viewProjMat;
    bufferData.prevViewProjMat = m_prevViewProjMat;

    bufferData.dimensions = Vec4u { uint32(MathUtil::Abs(m_width)), uint32(MathUtil::Abs(m_height)), 0, 1 };

    bufferData.cameraPosition = Vec4f(GetWorldTranslation(), 1.0f);
    bufferData.cameraDirection = Vec4f(m_direction, 1.0f);

    bufferData.cameraNear = m_near;
    bufferData.cameraFar = m_far;

    bufferData.cameraFov = m_fov;

    bufferData.jitter = m_jitter;

    // Save current view-projection as previous for next frame's velocity
    m_prevViewProjMat = m_viewProjMat;
}

#pragma endregion Camera

} // namespace Hyperion
