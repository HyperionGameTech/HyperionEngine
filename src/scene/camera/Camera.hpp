#ifndef HYPERION_V2_CAMERA_H
#define HYPERION_V2_CAMERA_H

#include <GameCounter.hpp>

#include <core/lib/Queue.hpp>

#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/Matrix4.hpp>
#include <math/Frustum.hpp>

#include <rendering/DrawProxy.hpp>

#include <atomic>
#include <mutex>

namespace hyperion {
namespace v2 {

class Engine;
class Framebuffer;

enum class CameraType
{
    NONE,
    PERSPECTIVE,
    ORTHOGRAPHIC
};

struct CameraCommand
{
    enum {
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

struct RenderCommand_UpdateCameraDrawProxy;

class Camera;

class CameraController
{
    friend class Camera;
public:
    CameraController(CameraType type);
    virtual ~CameraController() = default;

    CameraType GetType() const
        { return m_type; }

    virtual void OnAdded(Camera *camera)
        { m_camera = camera; }

    virtual void SetTranslation(const Vector3 &translation) { }
    virtual void SetNextTranslation(const Vector3 &translation) { }
    virtual void SetDirection(const Vector3 &direction) { }
    virtual void SetUpVector(const Vector3 &up) { }

    virtual void UpdateLogic(double dt) = 0;
    virtual void UpdateViewMatrix() = 0;
    virtual void UpdateProjectionMatrix() = 0;

    void PushCommand(const CameraCommand &command);

protected:
    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) {}

    void UpdateCommandQueue(GameCounter::TickUnit dt);

    Camera *m_camera;

    CameraType m_type;

    std::mutex m_command_queue_mutex;
    std::atomic_uint m_command_queue_count;
    Queue<CameraCommand> m_command_queue;
};

class PerspectiveCameraController;
class OrthoCameraController;
class FirstPersonCameraController;
class FollowCameraController;

class Camera :
    public EngineComponentBase<STUB_CLASS(Camera)>,
    public HasDrawProxy<STUB_CLASS(Camera)>
{
public:
    friend class CameraController;
    friend class PerspectiveCameraController;
    friend class OrthoCameraController;
    friend class FirstPersonCameraController;
    friend class FollowCameraController;

    friend struct RenderCommand_UpdateCameraDrawProxy;

    Camera();
    Camera(int width, int height);
    Camera(float fov, int width, int height, float _near, float _far);
    Camera(int width, int height, float left, float right, float bottom, float top, float _near, float _far);
    ~Camera();

    Handle<Framebuffer> &GetFramebuffer()
        { return m_framebuffer; }

    const Handle<Framebuffer> &GetFramebuffer() const
        { return m_framebuffer; }

    void SetFramebuffer(const Handle<Framebuffer> &framebuffer);

    CameraController *GetCameraController() const
        { return m_camera_controller.Get(); }

    void SetCameraController(UniquePtr<CameraController> &&camera_controller)
    {
        m_camera_controller = std::move(camera_controller);

        if (m_camera_controller) {
            m_camera_controller->OnAdded(this);
        }
    }

    void SetToPerspectiveProjection(
        Float fov, Float _near, Float _far
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
        Float left, Float right,
        Float bottom, Float top,
        Float _near, Float _far
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

    int GetWidth() const { return m_width; }
    void SetWidth(int width) { m_width = width; }
    int GetHeight() const { return m_height; }
    void SetHeight(int height) { m_height = height; }
    float GetNear() const { return m_near; }
    void SetNear(float _near) { m_near = _near; }
    float GetFar() const { return m_far; }
    void SetFar(float _far) { m_far = _far; }

    // perspective only
    float GetFov() const { return m_fov; }

    // ortho only
    float GetLeft() const { return m_left; }
    void SetLeft(float left) { m_left = left; }
    float GetRight() const { return m_right; }
    void SetRight(float right) { m_right = right; }
    float GetBottom() const { return m_bottom; }
    void SetBottom(float bottom) { m_bottom = bottom; }
    float GetTop() const { return m_top; }
    void SetTop(float top) { m_top = top; }

    const Vector3 &GetTranslation() const { return m_translation; }
    void SetTranslation(const Vector3 &translation);
    void SetNextTranslation(const Vector3 &translation);

    const Vector3 &GetDirection() const { return m_direction; }
    void SetDirection(const Vector3 &direction);

    const Vector3 &GetUpVector() const { return m_up; }
    void SetUpVector(const Vector3 &up);

    Vector3 GetSideVector() const { return m_up.Cross(m_direction); }

    Vector3 GetTarget() const { return m_translation + m_direction; }
    void SetTarget(const Vector3 &target) { SetDirection(target - m_translation); }

    void Rotate(const Vector3 &axis, float radians);

    /*! \brief For deserialization, allow modification of the object. */
    Frustum &GetFrustum() { return m_frustum; }
    const Frustum &GetFrustum() const { return m_frustum; }

    /*! \brief For deserialization, allow modification of the object. */
    Matrix4 &GetViewMatrix() { return m_view_mat; }
    const Matrix4 &GetViewMatrix() const { return m_view_mat; }
    void SetViewMatrix(const Matrix4 &view_mat);

    /*! \brief For deserialization, allow modification of the object. */
    Matrix4 &GetProjectionMatrix() { return m_proj_mat; }
    const Matrix4 &GetProjectionMatrix() const { return m_proj_mat; }
    void SetProjectionMatrix(const Matrix4 &proj_mat);

    /*! \brief For deserialization, allow modification of the object. */
    Matrix4 &GetViewProjectionMatrix() { return m_view_proj_mat; }
    const Matrix4 &GetViewProjectionMatrix() const { return m_view_proj_mat; }
    void SetViewProjectionMatrix(const Matrix4 &view_mat, const Matrix4 &proj_mat);

    /*! \brief Transform a 2D vector of x,y ranging from [0, 1] into ndc coordinates */
    Vector3 TransformScreenToNDC(const Vector2 &screen) const;

    /*! \brief Transform a 3D vector in NDC space into world coordinates */
    Vector4 TransformNDCToWorld(const Vector3 &ndc) const;

    /*! \brief Transform a 3D vector in world space into NDC space */
    Vector3 TransformWorldToNDC(const Vector3 &world) const;

    /*! \brief Transform a 3D vector in world space into screen space */
    Vector2 TransformWorldToScreen(const Vector3 &world) const;

    /*! \brief Transform a 3D vector in NDC into screen space */
    Vector2 TransformNDCToScreen(const Vector3 &ndc) const;

    /*! \brief Transform a 2D vector of x,y ranging from [0, 1] into world coordinates */
    Vector4 TransformScreenToWorld(const Vector2 &screen) const;

    Vector2 GetPixelSize() const;

    void Update(GameCounter::TickUnit dt);

    void UpdateMatrices();

    void Init();

protected:
    void UpdateViewMatrix();
    void UpdateProjectionatrix();
    void UpdateViewProjectionMatrix();

    UniquePtr<CameraController> m_camera_controller;

    Handle<Framebuffer> m_framebuffer;

    Vector3 m_translation, m_next_translation, m_direction, m_up;
    Matrix4 m_view_mat, m_proj_mat;
    Frustum m_frustum;

    int m_width, m_height;
    float m_near, m_far;

    // only for perspective
    float m_fov;

    // only for ortho
    float m_left, m_right, m_bottom, m_top;

private:
    Matrix4 m_view_proj_mat;
    Matrix4 m_previous_view_matrix;
};

} // namespace v2
} // namespace hyperion

#endif
