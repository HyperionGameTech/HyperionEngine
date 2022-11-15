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

class Camera :
    public EngineComponentBase<STUB_CLASS(Camera)>,
    public HasDrawProxy<STUB_CLASS(Camera)>
{
public:
    Camera(CameraType camera_type, int width, int height, float _near, float _far);
    virtual ~Camera();

    CameraType GetCameraType() const { return m_camera_type; }

    int GetWidth() const { return m_width; }
    void SetWidth(int width) { m_width = width; }
    int GetHeight() const { return m_height; }
    void SetHeight(int height) { m_height = height; }
    float GetNear() const { return m_near; }
    void SetNear(float _near) { m_near = _near; }
    float GetFar() const { return m_far; }
    void SetFar(float _far) { m_far = _far; }
    float GetFov() const { return m_fov; }

    const Vector3 &GetTranslation() const { return m_translation; }
    virtual void SetTranslation(const Vector3 &translation);
    virtual void SetNextTranslation(const Vector3 &translation);

    const Vector3 &GetDirection() const { return m_direction; }
    virtual void SetDirection(const Vector3 &direction);

    const Vector3 &GetUpVector() const { return m_up; }
    virtual void SetUpVector(const Vector3 &up);

    Vector3 GetSideVector() const { return m_up.Cross(m_direction); }

    Vector3 GetTarget() const { return m_translation + m_direction; }
    void SetTarget(const Vector3 &target) { SetDirection(target - m_translation); }

    void Rotate(const Vector3 &axis, float radians);

    Frustum &GetFrustum() { return m_frustum; }
    const Frustum &GetFrustum() const { return m_frustum; }

    Matrix4 &GetViewMatrix() { return m_view_mat; }
    const Matrix4 &GetViewMatrix() const { return m_view_mat; }
    void SetViewMatrix(const Matrix4 &view_mat);

    Matrix4 &GetProjectionMatrix() { return m_proj_mat; }
    const Matrix4 &GetProjectionMatrix() const { return m_proj_mat; }
    void SetProjectionMatrix(const Matrix4 &proj_mat);

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

    void Update(Engine *engine, GameCounter::TickUnit dt);

    virtual void UpdateLogic(double dt) = 0;

    virtual void UpdateViewMatrix() = 0;
    virtual void UpdateProjectionMatrix() = 0;
    void UpdateMatrices();

    /*! \brief Push a command to the camera in a thread-safe way. */
    void PushCommand(const CameraCommand &command);

    void Init(Engine *engine);

protected:
    virtual void RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt) {}

    void UpdateViewProjectionMatrix();

    CameraType m_camera_type;

    Vector3 m_translation, m_next_translation, m_direction, m_up;
    Matrix4 m_view_mat, m_proj_mat;
    Frustum m_frustum;

    int m_width, m_height;
    float m_near, m_far;
    float m_fov; // only for perspective

private:
    void UpdateCommandQueue(GameCounter::TickUnit dt);

    Matrix4 m_view_proj_mat;
    Matrix4 m_previous_view_matrix;

    std::mutex m_command_queue_mutex;
    std::atomic_uint m_command_queue_count;
    Queue<CameraCommand> m_command_queue;
};

} // namespace v2
} // namespace hyperion

#endif
