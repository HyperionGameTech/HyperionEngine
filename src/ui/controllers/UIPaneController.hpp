#ifndef HYPERION_V2_UI_CONTAINER_CONTROLLER_HPP
#define HYPERION_V2_UI_CONTAINER_CONTROLLER_HPP

#include <ui/controllers/UIController.hpp>
#include <ui/controllers/UIGridController.hpp>

#include <scene/Entity.hpp>
#include <math/BoundingBox.hpp>

namespace hyperion::v2 {

class Engine;

enum UIContainerHandleType
{
    UI_HANDLE_RIGHT,
    UI_HANDLE_BOTTOM,
    UI_HANDLE_CORNER
};

class UIPaneController : public UIGridController
{
public:
    UIPaneController();
    virtual ~UIPaneController() override = default;
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnEvent(const UIEvent &event) override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnTransformUpdate(const Transform &transform) override;
    virtual void OnDetachedFromScene(ID<Scene> scene) override;
    virtual void OnAttachedToScene(ID<Scene> scene) override;

    Vector4 GetHandleRect(const UIContainerHandleType handle_type);

    void SetHandleThickness(Float thickness)
    {
        m_handle_thickness = thickness;
    }
    Float GetHandleThickness()
    {
        return m_handle_thickness;
    }

private:
    bool TransformHandle(const Vector4 &bounds, Extent2D direction);
    inline bool IsMouseWithinHandle(const Vector2 &mouse, const Vector4 &handle);
    void CheckResizeHovering(const UIEvent &event);
    void CheckContainerResize();

    Vector2 m_mouse_click_position;
    Vector2 m_mouse_last_click;
    Extent2D m_drag_direction = { 0, 0 };
    Float m_handle_thickness = 0.01f;

    Handle<Camera> m_attached_camera;

protected:
    virtual bool CreateScriptedMethods() override;
};

} // namespace hyperion::v2

#endif
