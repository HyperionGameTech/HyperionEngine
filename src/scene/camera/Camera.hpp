/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/Mat4f.hpp>
#include <core/math/Frustum.hpp>
#include <core/math/Extent.hpp>

#include <input/InputHandler.hpp>

#include <rendering/RenderObject.hpp>

#include <scene/Entity.hpp>

namespace Hyperion {

class CameraStreamingVolume;
class RenderProxyCamera;
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
    NONE = 0x0,
    MATCH_WINDOW_SIZE = 0x1
};

HYP_MAKE_ENUM_FLAGS(CameraFlags)

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
class HYP_API CameraController : public ObjectBase
{
    friend class Camera;

    HYP_OBJECT_BODY(CameraController);

    CameraController();

public:
    explicit CameraController(CameraProjectionMode projectionMode);
    virtual ~CameraController() = default;

    HYP_METHOD(Property = "InputHandler")
    HYP_FORCE_INLINE const Handle<InputHandlerBase>& GetInputHandler() const
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
class HYP_API NullCameraController final : public CameraController
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
class HYP_API Camera final : public Entity
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

    HYP_METHOD(Property = "Flags", Editor = true)
    HYP_FORCE_INLINE EnumFlags<CameraFlags> GetCameraFlags() const
    {
        return m_cameraFlags;
    }

    HYP_METHOD(Property = "Flags", Editor = true)
    HYP_FORCE_INLINE void SetCameraFlags(EnumFlags<CameraFlags> flags)
    {
        m_cameraFlags = flags;
    }

    HYP_METHOD()
    void SetWindow(ApplicationWindow* window);

    HYP_METHOD(Property = "CameraControllers")
    HYP_FORCE_INLINE const Array<Handle<CameraController>>& GetCameraControllers() const
    {
        return m_cameraControllers;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<CameraController>& GetCameraController() const
    {
        return m_cameraControllers.Back();
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool HasActiveCameraController() const
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
        m_left = left;
        m_right = right;
        m_bottom = bottom;
        m_top = top;
        m_near = _near;
        m_far = _far;

        m_projMat = Mat4f::Orthographic(
            m_left, m_right,
            m_bottom, m_top,
            m_near, m_far);

        UpdateViewProjectionMatrix();
    }

    HYP_METHOD(Property = "Dimensions")
    HYP_FORCE_INLINE Vec2i GetDimensions() const
    {
        return Vec2i { m_width, m_height };
    }

    HYP_METHOD(Property = "Dimensions")
    HYP_FORCE_INLINE void SetDimensions(Vec2i dimensions)
    {
        m_width = dimensions.x;
        m_height = dimensions.y;

        UpdateProjectionMatrix();
    }

    HYP_METHOD(Property = "Near", Editor = true)
    HYP_FORCE_INLINE float GetNear() const
    {
        return m_near;
    }

    HYP_METHOD(Property = "Near", Editor = true)
    HYP_FORCE_INLINE void SetNear(float _near)
    {
        m_near = _near;
    }

    HYP_METHOD(Property = "Far", Editor = true)
    HYP_FORCE_INLINE float GetFar() const
    {
        return m_far;
    }

    HYP_METHOD(Property = "Far", Editor = true)
    HYP_FORCE_INLINE void SetFar(float _far)
    {
        m_far = _far;
    }

    // perspective only
    HYP_METHOD(Property = "FOV", Editor = true)
    HYP_FORCE_INLINE float GetFOV() const
    {
        return m_fov;
    }

    // perspective only
    HYP_METHOD(Property = "FOV", Editor = true)
    HYP_FORCE_INLINE void SetFOV(float fov)
    {
        m_fov = fov;
    }

    // ortho only
    HYP_METHOD(Property = "Left", Editor = true)
    HYP_FORCE_INLINE float GetLeft() const
    {
        return m_left;
    }

    // ortho only
    HYP_METHOD(Property = "Left", Editor = true)
    HYP_FORCE_INLINE void SetLeft(float left)
    {
        m_left = left;
    }

    // ortho only
    HYP_METHOD(Property = "Right", Editor = true)
    HYP_FORCE_INLINE float GetRight() const
    {
        return m_right;
    }

    // ortho only
    HYP_METHOD(Property = "Right", Editor = true)
    HYP_FORCE_INLINE void SetRight(float right)
    {
        m_right = right;
    }

    // ortho only
    HYP_METHOD(Property = "Bottom", Editor = true)
    HYP_FORCE_INLINE float GetBottom() const
    {
        return m_bottom;
    }

    // ortho only
    HYP_METHOD(Property = "Bottom", Editor = true)
    HYP_FORCE_INLINE void SetBottom(float bottom)
    {
        m_bottom = bottom;
    }

    HYP_METHOD(Property = "Top", Editor = true)
    HYP_FORCE_INLINE float GetTop() const
    {
        return m_top;
    }

    // ortho only
    HYP_METHOD(Property = "Top", Editor = true)
    HYP_FORCE_INLINE void SetTop(float top)
    {
        m_top = top;
    }

    // ortho only
    HYP_METHOD(Property = "Translation", Editor = true)
    HYP_FORCE_INLINE const Vec3f& GetTranslation() const
    {
        return m_translation;
    }

    HYP_METHOD(Property = "Translation", Editor = true)
    void SetTranslation(const Vec3f& translation);

    void SetNextTranslation(const Vec3f& translation);

    HYP_METHOD(Property = "Direction", Editor = true)
    HYP_FORCE_INLINE const Vec3f& GetDirection() const
    {
        return m_direction;
    }

    HYP_METHOD(Property = "Direction", Editor = true)
    void SetDirection(const Vec3f& direction);

    HYP_METHOD(Property = "Up", Editor = true)
    HYP_FORCE_INLINE const Vec3f& GetUpVector() const
    {
        return m_up;
    }

    HYP_METHOD(Property = "Up", Editor = true)
    HYP_FORCE_INLINE void SetUpVector(const Vec3f& up);

    HYP_METHOD()
    HYP_FORCE_INLINE Vec3f GetSideVector() const
    {
        return m_up.Cross(m_direction);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Vec3f GetTarget() const
    {
        return m_translation + m_direction;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetTarget(const Vec3f& target)
    {
        SetDirection(target - m_translation);
    }

    HYP_METHOD()
    void Rotate(const Vec3f& axis, float radians);

    HYP_FORCE_INLINE const Handle<CameraStreamingVolume>& GetStreamingVolume() const
    {
        return m_streamingVolume;
    }

    HYP_METHOD(Property = "Frustum", Editor = true)
    HYP_FORCE_INLINE const Frustum& GetFrustum() const
    {
        return m_frustum;
    }

    HYP_METHOD(Property = "Frustum", Editor = true)
    HYP_FORCE_INLINE void SetFrustum(const Frustum& frustum)
    {
        m_frustum = frustum;
    }

    HYP_METHOD(Property = "ViewMatrix", Editor = true)
    HYP_FORCE_INLINE const Mat4f& GetViewMatrix() const
    {
        return m_viewMat;
    }

    HYP_METHOD(Property = "ViewMatrix", Editor = true)
    void SetViewMatrix(const Mat4f& viewMat);

    HYP_METHOD(Property = "ViewMatrix", Editor = true)
    HYP_FORCE_INLINE const Mat4f& GetProjectionMatrix() const
    {
        return m_projMat;
    }

    HYP_METHOD(Property = "ViewMatrix", Editor = true)
    void SetProjectionMatrix(const Mat4f& projMat);

    HYP_METHOD()
    HYP_FORCE_INLINE const Mat4f& GetViewProjectionMatrix() const
    {
        return m_viewProjMat;
    }

    HYP_METHOD()
    void SetViewProjectionMatrix(const Mat4f& viewMat, const Mat4f& projMat);

    HYP_METHOD()
    HYP_FORCE_INLINE const Mat4f& GetPreviousViewProjectionMatrix() const
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

    void Update(float delta) override;
    void UpdateMatrices();

    void UpdateRenderProxy(RenderProxyCamera* proxy);

protected:
    void Init() override;

    void UpdateViewMatrix();
    void UpdateProjectionMatrix();
    void UpdateViewProjectionMatrix();

    EnumFlags<CameraFlags> m_cameraFlags;

    HYP_FIELD(Property = "MatchWindowSizeRatio", Editor)
    float m_matchWindowSizeRatio;

    HYP_FIELD(Property = "CameraControllers")
    Array<Handle<CameraController>> m_cameraControllers;

    Vec3f m_translation, m_nextTranslation, m_direction, m_up;
    Mat4f m_viewMat, m_projMat;
    Frustum m_frustum;

    int m_width, m_height;
    float m_near, m_far;

    // only for perspective
    float m_fov;

    // only for ortho
    float m_left, m_right, m_bottom, m_top;

private:
    /*! \internal For serialization only. */
    HYP_METHOD(Property = "CameraControllers")
    void SetCameraControllers(const Array<Handle<CameraController>>& cameraControllers);

    void UpdateMouseLocked();

    virtual void OnAddedToWorld(World* world) override;
    virtual void OnRemovedFromWorld(World* world) override;

    Mat4f m_viewProjMat;
    Mat4f m_prevViewProjMat;

    InputMouseLockScope m_mouseLockScope;

    Handle<CameraStreamingVolume> m_streamingVolume;

    WeakHandle<ApplicationWindow> m_window;

    DelegateHandler m_onMainWindowChangedHandle;
    DelegateHandler m_onWindowResizedHandle;
};

} // namespace Hyperion
