/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CAMERA_HPP
#define HYPERION_CAMERA_HPP

#include <core/Base.hpp>

#include <core/containers/Queue.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/Handle.hpp>

#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/Matrix4.hpp>
#include <math/Frustum.hpp>
#include <math/Extent.hpp>

#include <input/InputHandler.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <GameCounter.hpp>

#include <atomic>
#include <mutex>

namespace hyperion {

class Engine;

enum class CameraType
{
    NONE,
    PERSPECTIVE,
    ORTHOGRAPHIC
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

    union {
        struct {  // NOLINT(clang-diagnostic-microsoft-anon-tag)
            int mouse_x = 0,
                mouse_y = 0;
            float mx = 0.0f,
                  my = 0.0f; // in range -0.5f, 0.5f
        } mag_data;

        struct {  // NOLINT(clang-diagnostic-microsoft-anon-tag)
            int wheel_x = 0,
                wheel_y = 0;
        } scroll_data;

        struct {  // NOLINT(clang-diagnostic-microsoft-anon-tag)
            MovementType movement_type = CAMERA_MOVEMENT_NONE;
            float amount               = 1.0f;
        } movement_data;
    };
};

struct RENDER_COMMAND(UpdateCameraDrawProxy);

class Camera;

class HYP_API CameraController
{
    friend class Camera;

public:
    CameraController(CameraType type);
    virtual ~CameraController() = default;

    InputHandler *GetInputHandler() const
        { return m_input_handler.Get(); }

    void SetInputHandler(UniquePtr<InputHandler> &&input_handler)
        { m_input_handler = std::move(input_handler); }

    Camera *GetCamera() const
        { return m_camera; }

    CameraType GetType() const
        { return m_type; }

    virtual bool IsMouseLocked() const
        { return false; }

    virtual void SetTranslation(const Vec3f &translation) { }
    virtual void SetNextTranslation(const Vec3f &translation) { }
    virtual void SetDirection(const Vec3f &direction) { }
    virtual void SetUpVector(const Vec3f &up) { }

    virtual void OnAdded(Camera *camera) = 0;

    virtual void UpdateLogic(double dt) = 0;
    virtual void UpdateViewMatrix() = 0;
    virtual void UpdateProjectionMatrix() = 0;

    void PushCommand(const CameraCommand &command);

protected:
    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) {}

    void UpdateCommandQueue(GameCounter::TickUnit dt);

    void SetMouseLocked(bool mouse_locked);

    Camera                  *m_camera;

    UniquePtr<InputHandler> m_input_handler;

    CameraType              m_type;

    std::mutex              m_command_queue_mutex;
    std::atomic_uint        m_command_queue_count;
    Queue<CameraCommand>    m_command_queue;
};

class PerspectiveCameraController;
class OrthoCameraController;
class FirstPersonCameraController;
class FollowCameraController;

struct CameraDrawProxy
{
    Matrix4     view;
    Matrix4     projection;
    Matrix4     previous_view;
    Vec3f       position;
    Vec3f       direction;
    Vec3f       up;
    Extent2D    dimensions;
    float       clip_near;
    float       clip_far;
    float       fov;
    Frustum     frustum;

    uint64      visibility_bitmask;
    uint16      visibility_nonce;
};

class HYP_API Camera : public BasicObject<Camera>
{
public:
    friend class CameraController;
    friend class PerspectiveCameraController;
    friend class OrthoCameraController;
    friend class FirstPersonCameraController;
    friend class FollowCameraController;

    friend struct RENDER_COMMAND(UpdateCameraDrawProxy);

    Camera();
    Camera(int width, int height);
    Camera(float fov, int width, int height, float _near, float _far);
    Camera(int width, int height, float left, float right, float bottom, float top, float _near, float _far);
    ~Camera();

    const FramebufferRef &GetFramebuffer() const
        { return m_framebuffer; }

    void SetFramebuffer(const FramebufferRef &framebuffer);

    CameraController *GetCameraController() const
        { return m_camera_controller.Get(); }

    void SetCameraController(RC<CameraController> camera_controller)
    {
        m_camera_controller = std::move(camera_controller);

        if (m_camera_controller) {
            m_camera_controller->OnAdded(this);
        }
    }

    void SetToPerspectiveProjection(
        float fov, float _near, float _far
    )
    {
        m_fov  = fov;
        m_near = _near;
        m_far  = _far;

        m_proj_mat = Matrix4::Perspective(
            m_fov,
            m_width, m_height,
            m_near,  m_far
        );
    }

    void SetToOrthographicProjection(
        float left, float right,
        float bottom, float top,
        float _near, float _far
    )
    {
        m_left = left;     m_right = right;
        m_bottom = bottom; m_top = top;
        m_near = _near;    m_far = _far;

        m_proj_mat = Matrix4::Orthographic(
            m_left,   m_right,
            m_bottom, m_top,
            m_near,   m_far
        );
    }

    int GetWidth() const
        { return m_width; }

    void SetWidth(int width)
        { m_width = width; }

    int GetHeight() const
        { return m_height; }

    void SetHeight(int height)
        { m_height = height; }

    float GetNear() const
        { return m_near; }

    void SetNear(float _near)
        { m_near = _near; }

    float GetFar() const
        { return m_far; }

    void SetFar(float _far)
        { m_far = _far; }

    // perspective only
    float GetFOV() const
        { return m_fov; }

    // ortho only
    float GetLeft() const
        { return m_left; }

    void SetLeft(float left)
        { m_left = left; }

    float GetRight() const
        { return m_right; }

    void SetRight(float right)
        { m_right = right; }

    float GetBottom() const
        { return m_bottom; }

    void SetBottom(float bottom)
        { m_bottom = bottom; }

    float GetTop() const
        { return m_top; }
        
    void SetTop(float top)
        { m_top = top; }

    const Vec3f &GetTranslation() const
        { return m_translation; }

    void SetTranslation(const Vec3f &translation);
    void SetNextTranslation(const Vec3f &translation);

    const Vec3f &GetDirection() const
        { return m_direction; }

    void SetDirection(const Vec3f &direction);

    const Vec3f &GetUpVector() const
        { return m_up; }

    void SetUpVector(const Vec3f &up);

    Vec3f GetSideVector() const
        { return m_up.Cross(m_direction); }

    Vec3f GetTarget() const
        { return m_translation + m_direction; }

    void SetTarget(const Vec3f &target)
        { SetDirection(target - m_translation); }

    void Rotate(const Vec3f &axis, float radians);

    /*! \brief For deserialization, allow modification of the object. */
    Frustum &GetFrustum()
        { return m_frustum; }

    const Frustum &GetFrustum() const
        { return m_frustum; }

    /*! \brief For deserialization, allow modification of the object. */
    Matrix4 &GetViewMatrix()
        { return m_view_mat; }

    const Matrix4 &GetViewMatrix() const
        { return m_view_mat; }

    void SetViewMatrix(const Matrix4 &view_mat);

    /*! \brief For deserialization, allow modification of the object. */
    Matrix4 &GetProjectionMatrix()
        { return m_proj_mat; }

    const Matrix4 &GetProjectionMatrix() const
        { return m_proj_mat; }

    void SetProjectionMatrix(const Matrix4 &proj_mat);

    /*! \brief For deserialization, allow modification of the object. */
    Matrix4 &GetViewProjectionMatrix()
        { return m_view_proj_mat; }

    const Matrix4 &GetViewProjectionMatrix() const
        { return m_view_proj_mat; }

    void SetViewProjectionMatrix(const Matrix4 &view_mat, const Matrix4 &proj_mat);

    const Matrix4 &GetPreviousViewMatrix() const
        { return m_previous_view_matrix; }

    /*! \brief Transform a 2D vector of x,y ranging from [0, 1] into ndc coordinates */
    Vec3f TransformScreenToNDC(const Vec2f &screen) const;

    /*! \brief Transform a 3D vector in NDC space into world coordinates */
    Vec4f TransformNDCToWorld(const Vec3f &ndc) const;

    /*! \brief Transform a 3D vector in world space into NDC space */
    Vec3f TransformWorldToNDC(const Vec3f &world) const;

    /*! \brief Transform a 3D vector in world space into screen space */
    Vec2f TransformWorldToScreen(const Vec3f &world) const;

    /*! \brief Transform a 3D vector in NDC into screen space */
    Vec2f TransformNDCToScreen(const Vec3f &ndc) const;

    /*! \brief Transform a 2D vector of x,y ranging from [0, 1] into world coordinates */
    Vec4f TransformScreenToWorld(const Vec2f &screen) const;

    Vec2f GetPixelSize() const;

    const CameraDrawProxy &GetProxy() const
        { return m_proxy; }

    void Update(GameCounter::TickUnit dt);
    void UpdateMatrices();

    void Init();

protected:
    void UpdateViewMatrix();
    void UpdateProjectionMatrix();
    void UpdateViewProjectionMatrix();

    RC<CameraController>    m_camera_controller;

    FramebufferRef          m_framebuffer;

    Vec3f                   m_translation, m_next_translation, m_direction, m_up;
    Matrix4                 m_view_mat, m_proj_mat;
    Frustum                 m_frustum;

    int                     m_width, m_height;
    float                   m_near, m_far;

    // only for perspective
    float                   m_fov;

    // only for ortho
    float                   m_left, m_right, m_bottom, m_top;

private:
    Matrix4                 m_view_proj_mat;
    Matrix4                 m_previous_view_matrix;
    
    CameraDrawProxy         m_proxy;
};

} // namespace hyperion

#endif
