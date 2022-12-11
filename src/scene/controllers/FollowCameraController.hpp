#ifndef HYPERION_V2_BASIC_CHARACTER_CONTROLLER_H
#define HYPERION_V2_BASIC_CHARACTER_CONTROLLER_H

#include <scene/Controller.hpp>
#include <scene/camera/Camera.hpp>
#include <math/Ray.hpp>

namespace hyperion::v2 {

class Engine;

class BasicCharacterController : public Controller
{
public:
    BasicCharacterController();
    virtual ~BasicCharacterController() override = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

    virtual void OnDetachedFromScene(ID<Scene> id) override;
    virtual void OnAttachedToScene(ID<Scene> id) override;

private:
    RayTestResults m_ray_test_results;

    Handle<Camera> m_camera;
};

} // namespace hyperion::v2

#endif
