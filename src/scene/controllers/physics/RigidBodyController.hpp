#ifndef HYPERION_V2_RIGID_BODY_CONTROLLER_H
#define HYPERION_V2_RIGID_BODY_CONTROLLER_H

#include <scene/Controller.hpp>
#include <physics/RigidBody.hpp>

namespace hyperion::v2 {

class RigidBodyController : public Controller
{
public:
    static constexpr const char *controller_name = "RigidBodyController";

    RigidBodyController();

    RigidBodyController(
        RC<physics::PhysicsShape> &&shape,
        const physics::PhysicsMaterial &physics_material
    );

    virtual ~RigidBodyController() override = default;

    void SetPhysicsShape(RC<physics::PhysicsShape> &&shape);

    const physics::PhysicsMaterial &GetPhysicsMaterial() const
        { return m_physics_material; }

    void SetPhysicsMaterial(const physics::PhysicsMaterial &physics_material);

    Handle<physics::RigidBody> &GetRigidBody()
        { return m_rigid_body; }

    const Handle<physics::RigidBody> &GetRigidBody() const
        { return m_rigid_body; }
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

    virtual void OnDetachedFromScene(ID<Scene> id) override;
    virtual void OnAttachedToScene(ID<Scene> id) override;

    virtual void Serialize(fbom::FBOMObject &out) const override;
    virtual fbom::FBOMResult Deserialize(const fbom::FBOMObject &in) override;

protected:
    RC<physics::PhysicsShape> m_shape;
    physics::PhysicsMaterial m_physics_material;
    Handle<physics::RigidBody> m_rigid_body;

    Vector3 m_origin_offset;
};

} // namespace hyperion::v2

#endif
