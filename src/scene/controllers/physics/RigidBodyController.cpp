#include <scene/controllers/physics/RigidBodyController.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

RigidBodyController::RigidBodyController(
    UniquePtr<physics::PhysicsShape> &&shape,
    const physics::PhysicsMaterial &physics_material
) : Controller("RigidBodyController"),
    m_shape(std::move(shape)),
    m_physics_material(physics_material)
{
}

void RigidBodyController::OnAdded()
{
    m_rigid_body = CreateObject<physics::RigidBody>(
        std::move(m_shape),
        m_physics_material
    );

    InitObject(m_rigid_body);

    m_rigid_body->SetTransform(GetOwner()->GetTransform());    
}

void RigidBodyController::OnRemoved()
{
    m_rigid_body.Reset();
}

void RigidBodyController::OnAttachedToScene(ID<Scene> id)
{
    Engine::Get()->GetWorld()->GetPhysicsWorld().AddRigidBody(Handle<physics::RigidBody>(m_rigid_body));
}

void RigidBodyController::OnDetachedFromScene(ID<Scene> id)
{
    Engine::Get()->GetWorld()->GetPhysicsWorld().RemoveRigidBody(m_rigid_body);
}

void RigidBodyController::OnUpdate(GameCounter::TickUnit delta)
{
    GetOwner()->SetTransform(m_rigid_body->GetTransform());
}

} // namespace hyperion::v2