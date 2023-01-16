#ifndef HYPERION_V2_SHADOW_MAP_CONTROLLER_HPP
#define HYPERION_V2_SHADOW_MAP_CONTROLLER_HPP

#include <scene/Controller.hpp>
#include <core/Handle.hpp>
#include <core/Name.hpp>
#include <math/Transform.hpp>
#include <GameCounter.hpp>

namespace hyperion::v2 {

class ShadowMapRenderer;
class Scene;

class ShadowMapController : public Controller
{
public:
    static constexpr const char *controller_name = "ShadowMapController";

    ShadowMapController();
    virtual ~ShadowMapController() override = default;
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnTransformUpdate(const Transform &transform) override;

    virtual void OnAttachedToScene(ID<Scene> id) override;
    virtual void OnDetachedFromScene(ID<Scene> id) override;

protected:
    void AddShadowMapRenderer(const Handle<Scene> &scene);
    void RemoveShadowMapRenderer();
    void UpdateShadowCamera(const Transform &transform);

    WeakHandle<Scene> m_shadow_map_renderer_scene;
    Name m_shadow_map_renderer_name;
    ShadowMapRenderer *m_shadow_map_renderer;
    Handle<Light> m_light;
};

} // namespace hyperion::v2

#endif