#ifndef HYPERION_V2_UI_BUTTON_CONTROLLER_HPP
#define HYPERION_V2_UI_BUTTON_CONTROLLER_HPP

#include <scene/Controller.hpp>

#include <scene/Entity.hpp>
#include <math/BoundingBox.hpp>

namespace hyperion::v2 {

class Engine;

class UIButtonController : public Controller
{
public:
    UIButtonController();
    virtual ~UIButtonController() override = default;
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnTransformUpdate(const Transform &transform) override;
};

} // namespace hyperion::v2

#endif
