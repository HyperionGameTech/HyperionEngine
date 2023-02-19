#include <scene/controllers/physics/RigidBodyController.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

RigidBodyController::RigidBodyController()
    : Controller()
{
}

RigidBodyController::RigidBodyController(
    UniquePtr<physics::PhysicsShape> &&shape,
    const physics::PhysicsMaterial &physics_material
) : Controller(),
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
    
    m_origin_offset = GetOwner()->GetTranslation() - GetOwner()->GetWorldAABB().GetCenter();

    Transform transform;
    transform.SetTranslation(GetOwner()->GetWorldAABB().GetCenter());
    transform.SetRotation(GetOwner()->GetRotation());
    transform.SetScale(GetOwner()->GetScale());

    m_rigid_body->SetTransform(transform);
}

void RigidBodyController::OnRemoved()
{
    m_rigid_body.Reset();
}

void RigidBodyController::OnAttachedToScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        if (scene->IsWorldScene()) {
            g_engine->GetWorld()->GetPhysicsWorld().AddRigidBody(Handle<physics::RigidBody>(m_rigid_body));
        }
    }
}

void RigidBodyController::OnDetachedFromScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        if (scene->IsWorldScene()) {
            g_engine->GetWorld()->GetPhysicsWorld().RemoveRigidBody(m_rigid_body);
        }
    }
}

void RigidBodyController::OnUpdate(GameCounter::TickUnit delta)
{
    Transform transform = m_rigid_body->GetTransform();
    transform.SetTranslation(transform.GetTranslation() + m_origin_offset);
    GetOwner()->SetTransform(transform);
}

void RigidBodyController::SetPhysicsShape(UniquePtr<physics::PhysicsShape> &&shape)
{
    if (m_rigid_body) {
        m_rigid_body->SetShape(std::move(shape));
    } else {
        m_shape = std::move(shape);
    }
}

void RigidBodyController::SetPhysicsMaterial(const physics::PhysicsMaterial &physics_material)
{
    m_physics_material = physics_material;

    if (m_rigid_body) {
        m_rigid_body->SetPhysicsMaterial(physics_material);
    }
}

} // namespace hyperion::v2