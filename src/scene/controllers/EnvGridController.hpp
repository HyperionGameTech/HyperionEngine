#ifndef HYPERION_V2_ENV_GRID_CONTROLLER_HPP
#define HYPERION_V2_ENV_GRID_CONTROLLER_HPP

#include <scene/Controller.hpp>
#include <core/Handle.hpp>
#include <core/Name.hpp>
#include <math/Transform.hpp>
#include <GameCounter.hpp>

namespace hyperion::v2 {

class EnvGrid;
class Scene;

class EnvGridController : public Controller
{
public:
    static constexpr const char *controller_name = "EnvGridController";

    EnvGridController();
    virtual ~EnvGridController() override = default;
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnTransformUpdate(const Transform &transform) override;

    virtual void OnAttachedToScene(ID<Scene> id) override;
    virtual void OnDetachedFromScene(ID<Scene> id) override;

protected:
    void AddEnvGridRenderer(const Handle<Scene> &scene);
    void RemoveEnvGridRenderer();
    void UpdateGridTransform(const Transform &transform);

    WeakHandle<Scene> m_env_grid_renderer_scene;
    Name m_env_grid_renderer_name;
    EnvGrid *m_env_grid_renderer;
};

} // namespace hyperion::v2

#endif