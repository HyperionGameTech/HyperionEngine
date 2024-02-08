#include <scene/controllers/physics/RigidBodyController.hpp>
#include <Engine.hpp>

#include <asset/serialization/fbom/FBOMObject.hpp>
#include <asset/serialization/fbom/marshals/PhysicsShapeMarshal.hpp>

namespace hyperion::v2 {

RigidBodyController::RigidBodyController()
    : Controller()
{
}

RigidBodyController::RigidBodyController(
    RC<physics::PhysicsShape> &&shape,
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

    // @TODO Update for new ECS
    
    // m_origin_offset = GetOwner()->GetTranslation() - GetOwner()->GetWorldAABB().GetCenter();

    // Transform transform;
    // transform.SetTranslation(GetOwner()->GetWorldAABB().GetCenter());
    // transform.SetRotation(GetOwner()->GetRotation());
    // transform.SetScale(GetOwner()->GetScale());

    // m_rigid_body->SetTransform(transform);
}

void RigidBodyController::OnRemoved()
{
    m_rigid_body.Reset();
}

void RigidBodyController::OnAttachedToScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        if (scene->IsWorldScene()) {
            g_engine->GetWorld()->GetPhysicsWorld().AddRigidBody(m_rigid_body);
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

void RigidBodyController::Serialize(fbom::FBOMObject &out) const
{
    out.SetProperty("controller_name", fbom::FBOMString(), Memory::StrLen(controller_name), controller_name);

    if (m_shape) {
        out.AddChild(*m_shape);
    }
    
    out.SetProperty("physics_shape.mass", fbom::FBOMData::FromFloat(m_physics_material.mass));
}

fbom::FBOMResult RigidBodyController::Deserialize(const fbom::FBOMObject &in)
{
    in.GetProperty("physics_shape.mass").ReadFloat(&m_physics_material.mass);

    
    for (auto &sub_object : *in.nodes) {
        if (sub_object.GetType().IsOrExtends("PhysicsShape")) {
            m_shape = sub_object.deserialized.Get<physics::PhysicsShape>();
        }
    }

    return fbom::FBOMResult::FBOM_OK;
}

void RigidBodyController::OnUpdate(GameCounter::TickUnit delta)
{
    // @TODO Update for new ECS

    Transform transform = m_rigid_body->GetTransform();
    transform.SetTranslation(transform.GetTranslation() + m_origin_offset);
    // GetOwner()->SetTransform(transform);
}

void RigidBodyController::SetPhysicsShape(RC<physics::PhysicsShape> &&shape)
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