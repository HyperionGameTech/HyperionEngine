#ifndef HYPERION_V2_RIGID_BODY_CONTROLLER_H
#define HYPERION_V2_RIGID_BODY_CONTROLLER_H

#include <scene/Controller.hpp>
#include <physics/RigidBody.hpp>

namespace hyperion::v2 {

class RigidBodyController : public Controller
{
public:
    RigidBodyController(
        UniquePtr<physics::PhysicsShape> &&shape,
        const physics::PhysicsMaterial &physics_material
    );

    virtual ~RigidBodyController() override = default;

    Handle<physics::RigidBody> &GetRigidBody()
        { return m_rigid_body; }

    const Handle<physics::RigidBody> &GetRigidBody() const
        { return m_rigid_body; }
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

    virtual void OnDetachedFromScene(ID<Scene> id) override;
    virtual void OnAttachedToScene(ID<Scene> id) override;

protected:
    UniquePtr<physics::PhysicsShape> m_shape;
    physics::PhysicsMaterial m_physics_material;
    Handle<physics::RigidBody> m_rigid_body;
};

} // namespace hyperion::v2

#endif
