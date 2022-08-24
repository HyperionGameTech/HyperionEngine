#ifndef HYPERION_V2_BASIC_CHARACTER_CONTROLLER_H
#define HYPERION_V2_BASIC_CHARACTER_CONTROLLER_H

#include "../Controller.hpp"
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

private:
    RayTestResults m_ray_test_results;
};

} // namespace hyperion::v2

#endif
