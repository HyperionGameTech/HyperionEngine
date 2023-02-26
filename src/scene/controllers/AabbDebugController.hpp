#ifndef HYPERION_V2_AABB_DEBUG_CONTROLLER_H
#define HYPERION_V2_AABB_DEBUG_CONTROLLER_H

#include "../Controller.hpp"

#include <scene/Entity.hpp>
#include <math/BoundingBox.hpp>

namespace hyperion::v2 {

class Engine;

class AABBDebugController : public Controller
{
public:
    static constexpr const char *controller_name = "AABBDebugController";

    AABBDebugController();
    virtual ~AABBDebugController() override = default;
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnTransformUpdate(const Transform &transform) override;

    virtual void OnAttachedToScene(ID<Scene> id) override;
    virtual void OnDetachedFromScene(ID<Scene> id) override;

    virtual void Serialize(fbom::FBOMObject &out) const override;
    virtual fbom::FBOMResult Deserialize(const fbom::FBOMObject &in) override;

protected:
    Handle<Entity> m_aabb_entity;
    BoundingBox m_aabb;
};

} // namespace hyperion::v2

#endif
