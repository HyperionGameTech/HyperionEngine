#ifndef HYPERION_V2_UI_CONTAINER_CONTROLLER_HPP
#define HYPERION_V2_UI_CONTAINER_CONTROLLER_HPP

#include <ui/controllers/UIController.hpp>
#include <ui/controllers/UIGridController.hpp>

#include <scene/Entity.hpp>
#include <math/BoundingBox.hpp>

namespace hyperion::v2 {

class Engine;

class UIContainerController : public UIGridController
{
public:
    static constexpr const char *controller_name = "UIContainerController";

    UIContainerController();
    virtual ~UIContainerController() override = default;
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnEvent(const UIEvent &event) override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnTransformUpdate(const Transform &transform) override;
    virtual void OnDetachedFromScene(ID<Scene> id) override;
    virtual void OnAttachedToScene(ID<Scene> id) override;

protected:
    virtual bool CreateScriptedMethods() override;
};

} // namespace hyperion::v2

#endif
