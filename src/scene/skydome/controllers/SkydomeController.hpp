#ifndef HYPERION_V2_SKYDOME_CONTROLLER_HPP
#define HYPERION_V2_SKYDOME_CONTROLLER_HPP

#include <scene/Controller.hpp>
#include <core/Handle.hpp>
#include <scene/NodeProxy.hpp>

namespace hyperion::v2 {

class Entity;
class Mesh;
class SkydomeRenderer;

class SkydomeController : public Controller
{
public:
    static constexpr const char *controller_name = "SkydomeController";

    SkydomeController();
    virtual ~SkydomeController() override = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

    virtual void OnDetachedFromScene(ID<Scene> id) override;
    virtual void OnAttachedToScene(ID<Scene> id) override;

    virtual void Serialize(fbom::FBOMObject &out) const override;
    virtual fbom::FBOMResult Deserialize(const fbom::FBOMObject &in) override;

protected:
    SkydomeRenderer *m_skydome_renderer;
    Handle<Entity>  m_dome;
};

} // namespace hyperion::v2

#endif