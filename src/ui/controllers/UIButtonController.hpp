#ifndef HYPERION_V2_UI_BUTTON_CONTROLLER_HPP
#define HYPERION_V2_UI_BUTTON_CONTROLLER_HPP

#include <ui/controllers/UIController.hpp>

#include <scene/Entity.hpp>
#include <math/BoundingBox.hpp>

namespace hyperion::v2 {

class Engine;

class UIButtonController : public UIController
{
public:
    UIButtonController();
    virtual ~UIButtonController() override = default;
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnEvent(const UIEvent &event) override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnTransformUpdate(const Transform &transform) override;

protected:
    virtual bool CreateScriptedMethods() override;
};

} // namespace hyperion::v2

#endif
