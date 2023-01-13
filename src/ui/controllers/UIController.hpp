#ifndef HYPERION_V2_UI_CONTROLLER_HPP
#define HYPERION_V2_UI_CONTROLLER_HPP

#include <scene/Controller.hpp>

#include <scene/Entity.hpp>
#include <math/BoundingBox.hpp>

namespace hyperion {

class SystemEvent;

} // namespace hyperion

namespace hyperion::v2 {

class Engine;

struct UIEvent
{
    enum class Type
    {
        NONE,

        MOUSE_DOWN,
        MOUSE_UP,
        MOUSE_DRAG,
        MOUSE_HOVER,
        MOUSE_HOVER_LOST,

        CLICK
    };

    Type type;
    Vector2 mouse_position;
    const ApplicationWindow *window = nullptr;
    const SystemEvent *original_event = nullptr;

    Type GetType() const
        { return type; }

    const SystemEvent *GetOriginalEvent() const
        { return original_event; }
    const ApplicationWindow *GetWindow() const
        { return window; }

    const Vector2 &GetMousePosition() const
        { return mouse_position; }
};

class UIController : public Controller
{
public:
    UIController(bool receives_update = true);
    virtual ~UIController() override = default;

    void SetGridOffset(Extent2D offset)
    {
        m_grid_offset = offset;
    }

    Extent2D GetGridOffset()
    {
        return m_grid_offset;
    }

    virtual void OnEvent(const UIEvent &event) = 0;
protected:
    Extent2D m_grid_offset = { 1, 1 };
};

} // namespace hyperion::v2

#endif
