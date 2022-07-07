#ifndef ORTHO_CAMERA_H
#define ORTHO_CAMERA_H

#include "Camera.hpp"

namespace hyperion {
class OrthoCamera : public Camera {
public:
    OrthoCamera(
        int width,    int height,
        float left,   float right,
        float bottom, float top,
        float _near,  float _far
    );
    virtual ~OrthoCamera() = default;

    float GetLeft() const        { return m_left; }
    void SetLeft(float left)     { m_left = left; }
    float GetRight() const       { return m_right; }
    void SetRight(float right)   { m_right = right; }
    float GetBottom() const      { return m_bottom; }
    void SetBottom(float bottom) { m_bottom = bottom; }
    float GetTop() const         { return m_top; }
    void SetTop(float top)       { m_top = top; }

    void Set(float left, float right, float bottom, float top, float _near, float _far)
    {
        m_left   = left;
        m_right  = right;
        m_bottom = bottom;
        m_top    = top;
        m_near   = _near;
        m_far    = _far;

        SetProjectionMatrix(Matrix4::Orthographic(
            m_left,   m_right,
            m_bottom, m_top,
            m_near,   m_far
        ));
    }

    virtual void UpdateLogic(double dt) override;
    virtual void UpdateViewMatrix() override;
    virtual void UpdateProjectionMatrix() override;

private:
    float m_left, m_right, m_bottom, m_top;
};
}

#endif
