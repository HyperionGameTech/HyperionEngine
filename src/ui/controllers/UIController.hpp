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
        MOUSE_UP
    };

    Type type;
    const SystemEvent *original_event = nullptr;
};

class UIController : public Controller
{
public:
    UIController(const String &name, bool receives_update = true);
    virtual ~UIController() override = default;

    virtual void OnEvent(const UIEvent &event) = 0;
};

} // namespace hyperion::v2

#endif
