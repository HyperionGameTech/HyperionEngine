/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>
#include <Core/Math/Mat4f.hpp>
#include <Core/Math/Frustum.hpp>
#include <Core/Math/Extent.hpp>
#include <Core/Math/Ray.hpp>

#include <Input/InputHandler.hpp>

#include <Rendering/RenderTypes.hpp>

#include <Scene/Entity.hpp>

namespace Hyperion {

class CameraStreamingVolume;
struct RenderProxyCamera;
class ApplicationWindow;

HYP_ENUM()
enum class CameraProjectionMode : uint32
{
    NONE = 0,
    PERSPECTIVE = 1,
    ORTHOGRAPHIC = 2
};

HYP_ENUM()
enum class CameraFlags : uint32
{
    None = 0x0,              //!< @editor=false
    MatchWindowSize = 0x1,   //!< @title="Match window dimensions" @description="Camera dimensions will be matched to the application's window size."
    HasStreamingVolume = 0x2 //!< @title="Acts as streaming source" @description="If enabled, triggers content to stream in based on camera distance"
};

HYP_MAKE_ENUM_FLAGS(CameraFlags);

HYP_STRUCT()
struct CameraOrthoRect
{
    HYP_STRUCT_BODY(CameraOrthoRect);

    HYP_FIELD()
    float left = 0.0f;

    HYP_FIELD()
    float right = 0.0f;

    HYP_FIELD()
    float bottom = 0.0f;

    HYP_FIELD()
    float top = 0.0f;
};

struct CameraCommand
{
    enum
    {
        CAMERA_COMMAND_NONE,
        CAMERA_COMMAND_MAG,
        CAMERA_COMMAND_SCROLL,
        CAMERA_COMMAND_MOVEMENT
    } command;

    enum MovementType
    {
        CAMERA_MOVEMENT_NONE,
        CAMERA_MOVEMENT_LEFT,
        CAMERA_MOVEMENT_RIGHT,
        CAMERA_MOVEMENT_FORWARD,
        CAMERA_MOVEMENT_BACKWARD
    };

    union
    {
        struct
        { // NOLINT(clang-diagnostic-microsoft-anon-tag)
            int mouseX = 0,
                mouseY = 0;
            float mx = 0.0f,
                  my = 0.0f; // in range -0.5f, 0.5f
        } magData;

        struct
        { // NOLINT(clang-diagnostic-microsoft-anon-tag)
            int wheelX = 0,
                wheelY = 0;
        } scrollData;

        struct
        { // NOLINT(clang-diagnostic-microsoft-anon-tag)
            MovementType movementType = CAMERA_MOVEMENT_NONE;
            float amount = 1.0f;
        } movementData;
    };
};

class Camera;

HYP_CLASS(Abstract)
class ENGINE_API CameraController : public ObjectBase
{
    friend class Camera;

    HYP_OBJECT_BODY(CameraController);

    CameraController();

public:
    explicit CameraController(CameraProjectionMode projectionMode);
    virtual ~CameraController() = default;

    HYP_METHOD(Property = "InputHandler")
    const Handle<InputHandlerBase>& GetInputHandler() const
    {
        return m_inputHandler;
    }

    HYP_METHOD(Property = "InputHandler")
    void SetInputHandler(const Handle<InputHandlerBase>& inputHandler);

    HYP_METHOD(Property = "Camera")
    Camera* GetCamera() const
    {
        return m_camera;
    }

    HYP_METHOD(Property = "ProjectionMode")
    CameraProjectionMode GetProjectionMode() const
    {
        return m_projectionMode;
    }

    HYP_METHOD()
    virtual bool IsMouseLockAllowed() const
    {
        return false;
    }

    HYP_METHOD()
    bool IsMouseLockRequested() const
    {
        return m_mouseLockRequested;
    }

    HYP_METHOD()
    virtual void SetTranslation(const Vec3f& translation)
    {
    }

    HYP_METHOD()
    virtual void SetNextTranslation(const Vec3f& translation)
    {
    }

    HYP_METHOD()
    virtual void SetDirection(const Vec3f& direction)
    {
    }

    HYP_METHOD()
    virtual void SetUpVector(const Vec3f& up)
    {
    }

    virtual void UpdateLogic(double delta) = 0;
    virtual void UpdateViewMatrix() = 0;
    virtual void UpdateProjectionMatrix() = 0;

protected:
    virtual void OnAdded();
    virtual void OnRemoved();

    virtual void OnActivated();
    virtual void OnDeactivated();

    void SetIsMouseLockRequested(bool mouseLockRequested);

    HYP_FIELD(Property = "Camera", Transient)
    Camera* m_camera;

    HYP_FIELD(Property = "InputHandler", Transient)
    Handle<InputHandlerBase> m_inputHandler;

    HYP_FIELD(Property = "ProjectionMode", Editor = true)
    CameraProjectionMode m_projectionMode;

    bool m_mouseLockRequested;

private:
    void OnAdded(Camera* camera);
};

HYP_CLASS()
class ENGINE_API NullCameraController final : public CameraController
{
    HYP_OBJECT_BODY(NullCameraController);

public:
    NullCameraController();
    virtual ~NullCameraController() override = default;

    virtual void UpdateLogic(double delta) override;
    virtual void UpdateViewMatrix() override;
    virtual void UpdateProjectionMatrix() override;

private:
    void Init() override;
};

class PerspectiveCameraController;
class OrthoCameraController;
class FirstPersonCameraController;
class FollowCameraController;

HYP_CLASS()
class ENGINE_API Camera final : public Entity
{
    HYP_OBJECT_BODY(Camera);

public:
    friend class CameraController;
    friend class PerspectiveCameraController;
    friend class OrthoCameraController;
    friend class FirstPersonCameraController;
    friend class FollowCameraController;

    Camera();
    
    Camera(int width, int height);
    Camera(float fov, int width, int height, float _near, float _far);
    Camera(int width, int height, float left, float right, float bottom, float top, float _near, float _far);

    ~Camera() override;

    HYP_METHOD(Property = "Flags", Editor = true, Serialize = true)
    EnumFlags<CameraFlags> GetCameraFlags() const
    {
        return m_cameraFlags;
    }

    HYP_METHOD(Property = "Flags", Editor = true, Serialize = true)
    void SetCameraFlags(EnumFlags<CameraFlags> flags);

    HYP_METHOD(Property = "CameraControllers", Serialize = true)
    const Array<Handle<CameraController>>& GetCameraControllers() const
    {
        return m_cameraControllers;
    }

    HYP_METHOD(Property = "CameraControllers", Serialize = true)
    const Handle<CameraController>& GetCameraController() const
    {
        return m_cameraControllers.Back();
    }

    HYP_METHOD()
    bool HasActiveCameraController() const
    {
        return m_cameraControllers.Size() > 1;
    }

    HYP_METHOD()
    void AddCameraController(const Handle<CameraController>& cameraController, int index = -1);

    HYP_METHOD()
    bool RemoveCameraController(const Handle<CameraController>& cameraController);

    void SetToPerspectiveProjection(
        float fov, float _near, float _far)
    {
        m_fov = fov;
        m_near = _near;
        m_far = _far;

        m_projMat = Mat4f::Perspective(
            m_fov,
            m_width, m_height,
            m_near, m_far);

        UpdateViewProjectionMatrix();
    }

    void SetToOrthographicProjection(
        float left, float right,
        float bottom, float top,
        float _near, float _far)
    {
        m_orthoRect.left = left;
        m_orthoRect.right = right;
        m_orthoRect.bottom = bottom;
        m_orthoRect.top = top;

        m_near = _near;
        m_far = _far;

        m_projMat = Mat4f::Orthographic(
            m_orthoRect.left, m_orthoRect.right,
            m_orthoRect.bottom, m_orthoRect.top,
            m_near, m_far);

        UpdateViewProjectionMatrix();
    }

    HYP_METHOD(Property = "Dimensions", Editor = true, Serialize = true)
    Vec2i GetDimensions() const
    {
        return Vec2i { m_width, m_height };
    }

    HYP_METHOD(Property = "Dimensions", Editor = true, Serialize = true)
    void SetDimensions(Vec2i dimensions)
    {
        m_width = dimensions.x;
        m_height = dimensions.y;

        UpdateProjectionMatrix();
    }

    HYP_METHOD(Property = "NearClip", Editor = true, Serialize = true)
    float GetNearClip() const
    {
        return m_near;
    }

    HYP_METHOD(Property = "NearClip", Editor = true, Serialize = true)
    void SetNearClip(float _near)
    {
        m_near = _near;
    }

    HYP_METHOD(Property = "FarClip", Editor = true, Serialize = true)
    float GetFarClip() const
    {
        return m_far;
    }

    HYP_METHOD(Property = "FarClip", Editor = true, Serialize = true)
    void SetFarClip(float _far)
    {
        m_far = _far;
    }

    // perspective only
    HYP_METHOD(Property = "FOV", Editor = true, Serialize = true)
    float GetFOV() const
    {
        return m_fov;
    }

    // perspective only
    HYP_METHOD(Property = "FOV", Editor = true, Serialize = true)
    void SetFOV(float fov)
    {
        m_fov = fov;
    }

    void SetNextTranslation(const Vec3f& translation);

    HYP_METHOD(Property = "Direction", Editor = true, Serialize = false)
    Vec3f GetDirection() const
    {
        return GetWorldRotation().RotateVector(Vec3f::UnitZ());
    }

    HYP_METHOD(Property = "Direction", Editor = true, Serialize = false)
    void SetDirection(const Vec3f& direction);

    HYP_METHOD(Property = "UpVector", Editor = true, Serialize = false)
    Vec3f GetUpVector() const
    {
        return Vec3f::UnitY();
    }

    HYP_METHOD(Property = "UpVector", Editor = true, Serialize = false)
    void SetUpVector(const Vec3f& up);

    HYP_METHOD(Property = "OrthoRect", Editor = true, Serialize = true)
    const CameraOrthoRect& GetOrthoRect() const
    {
        return m_orthoRect;
    }

    HYP_METHOD(Property = "OrthoRect", Editor = true, Serialize = true)
    void SetOrthoRect(const CameraOrthoRect& orthoRect)
    {
        m_orthoRect = orthoRect;
        UpdateProjectionMatrix();
    }

    HYP_METHOD()
    Vec3f GetSideVector() const
    {
        return GetUpVector().Cross(GetDirection());
    }

    HYP_METHOD()
    Vec3f GetTarget() const
    {
        return GetWorldTranslation() + GetDirection();
    }

    HYP_METHOD()
    void SetTarget(const Vec3f& target)
    {
        SetDirection(target - GetWorldTranslation());
    }

    HYP_METHOD()
    void Rotate(const Vec3f& axis, float radians);

    const Handle<CameraStreamingVolume>& GetStreamingVolume() const
    {
        return m_streamingVolume;
    }

    HYP_METHOD(Property = "Frustum", Editor = true, Serialize = false)
    const Frustum& GetFrustum() const
    {
        return m_frustum;
    }

    HYP_METHOD(Property = "Frustum", Editor = true, Serialize = false)
    void SetFrustum(const Frustum& frustum)
    {
        m_frustum = frustum;
    }

    HYP_METHOD(Property = "ViewMatrix", Editor = true, Serialize = false)
    const Mat4f& GetViewMatrix() const
    {
        return m_viewMat;
    }

    HYP_METHOD(Property = "ViewMatrix", Editor = true, Serialize = false)
    void SetViewMatrix(const Mat4f& viewMat);

    HYP_METHOD(Property = "ViewMatrix", Editor = true, Serialize = false)
    const Mat4f& GetProjectionMatrix() const
    {
        return m_projMat;
    }

    HYP_METHOD(Property = "ViewMatrix", Editor = true, Serialize = false)
    void SetProjectionMatrix(const Mat4f& projMat);

    HYP_METHOD()
    const Mat4f& GetViewProjectionMatrix() const
    {
        return m_viewProjMat;
    }

    HYP_METHOD()
    void SetViewProjectionMatrix(const Mat4f& viewMat, const Mat4f& projMat);

    HYP_METHOD()
    const Mat4f& GetPreviousViewProjectionMatrix() const
    {
        return m_prevViewProjMat;
    }

    /*! \brief Transform a 2D vector of x,y ranging from [0, 1] into ndc coordinates */
    HYP_METHOD()
    Vec3f TransformScreenToNDC(const Vec2f& screen) const;

    /*! \brief Transform a 3D vector in NDC space into world coordinates */
    HYP_METHOD()
    Vec4f TransformNDCToWorld(const Vec3f& ndc) const;

    /*! \brief Transform a 3D vector in world space into NDC space */
    HYP_METHOD()
    Vec3f TransformWorldToNDC(const Vec3f& world) const;

    /*! \brief Transform a 3D vector in world space into screen space */
    HYP_METHOD()
    Vec2f TransformWorldToScreen(const Vec3f& world) const;

    /*! \brief Transform a 3D vector in NDC into screen space */
    HYP_METHOD()
    Vec2f TransformNDCToScreen(const Vec3f& ndc) const;

    /*! \brief Transform a 2D vector of x,y ranging from [0, 1] into world coordinates */
    HYP_METHOD()
    Vec4f TransformScreenToWorld(const Vec2f& screen) const;

    HYP_METHOD()
    Vec2f GetPixelSize() const;

    HYP_METHOD()
    Ray GetPickRay(const Vec2f& screen) const;

    void Update(float delta) override;
    void UpdateMatrices();

    void UpdateRenderProxy(RenderProxyCamera* proxy);

protected:
    void Init() override;

    void UpdateMatchWindowSize();
    void UpdateStreamingVolume();

    void OnTransformUpdated() override;

    void UpdateViewMatrix();
    void UpdateProjectionMatrix();
    void UpdateViewProjectionMatrix();

    void UpdateJitter();

    EnumFlags<CameraFlags> m_cameraFlags;

    HYP_FIELD(Property = "MatchWindowSizeRatio", Editor)
    float m_matchWindowSizeRatio;

    HYP_FIELD(Property = "CameraControllers")
    Array<Handle<CameraController>> m_cameraControllers;

    Vec3f m_nextTranslation;
    Mat4f m_viewMat, m_projMat;
    Frustum m_frustum;

    int m_width, m_height;
    float m_near, m_far;

    // only for perspective
    HYP_FIELD(Property = "FOV", Editor, Serialize)
    float m_fov;

    HYP_FIELD(Property = "OrthoRect", Editor, Serialize)
    CameraOrthoRect m_orthoRect;

private:
    /*! \internal For serialization only. */
    HYP_METHOD(Property = "CameraControllers")
    void SetCameraControllers(const Array<Handle<CameraController>>& cameraControllers);

    void UpdateMouseLocked();

    virtual void OnAddedToWorld(World* world) override;
    virtual void OnRemovedFromWorld(World* world) override;

    Mat4f m_viewProjMat;
    Mat4f m_prevViewProjMat;

    Vec4f m_jitter = Vec4f::Zero();
    uint32 m_jitterFrameCounter = 0;

    InputMouseLockScope m_mouseLockScope;

    Handle<CameraStreamingVolume> m_streamingVolume;
    bool m_streamingVolumeAdded;

    DelegateHandler m_onMainWindowChangedHandle;
    DelegateHandler m_onWindowResizedHandle;
};

} // namespace Hyperion
