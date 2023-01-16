#ifndef HYPERION_V2_LIGHT_CONTROLLER_HPP
#define HYPERION_V2_LIGHT_CONTROLLER_HPP

#include <scene/Controller.hpp>
#include <core/Handle.hpp>

namespace hyperion::v2 {

class Light;

class LightController : public Controller
{
public:
    static constexpr const char *controller_name = "LightController";

    LightController(const Handle<Light> &light);
    virtual ~LightController() override = default;

    const Handle<Light> &GetLight() const
        { return m_light; }
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnTransformUpdate(const Transform &transform) override;

    virtual void OnAttachedToScene(ID<Scene> id) override;
    virtual void OnDetachedFromScene(ID<Scene> id) override;

protected:
    Handle<Light> m_light;
};

} // namespace hyperion::v2

#endif