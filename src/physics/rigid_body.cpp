#include "rigid_body.h"
#include "physics_manager.h"
#include "../math/matrix_util.h"
#include "../animation/bone.h"
#include "../rendering/renderers/bounding_box_renderer.h"

#include "../bullet_math_util.h"
#include "btBulletDynamicsCommon.h"

namespace hyperion {
namespace physics {
static Matrix4 CalculateTransformMatrix(const Vector3 &position, Quaternion orientation)
{
    orientation.Invert();

    Matrix4 R, T;

    MatrixUtil::ToRotation(R, orientation);
    MatrixUtil::ToTranslation(T, position);

    return R * T;
}

static Matrix3 CalculateInverseInertiaWorldMatrix(const Matrix3 &iit_body, const Matrix4 &transform)
{
    auto t4 = transform(0, 0) * iit_body(0, 0) +
        transform(0, 1) * iit_body(1, 0) +
        transform(0, 2) * iit_body(2, 0);
    auto t9 = transform(0, 0) * iit_body(0, 1) +
        transform(0, 1) * iit_body(1, 1) +
        transform(0, 2) * iit_body(2, 1);
    auto t14 = transform(0, 0) * iit_body(0, 2) +
        transform(0, 1)* iit_body(1, 2) +
        transform(0, 2) * iit_body(2, 2);
    auto t28 = transform(1, 0) * iit_body(0, 0) +
        transform(1, 1) * iit_body(1, 0) +
        transform(1, 2) * iit_body(2, 0);
    auto t33 = transform(1, 0) * iit_body(0, 1) +
        transform(1, 1) * iit_body(1, 1) +
        transform(1, 2) * iit_body(2, 1);
    auto t38 = transform(1, 0) * iit_body(0, 2) +
        transform(1, 1) * iit_body(1, 2) +
        transform(1, 2) * iit_body(2, 2);
    auto t52 = transform(2, 0) * iit_body(0, 0) +
        transform(2, 1) * iit_body(1, 0) +
        transform(2, 2) * iit_body(2, 0);
    auto t57 = transform(2, 0) * iit_body(0, 1) +
        transform(2, 1) * iit_body(1, 1) +
        transform(2, 2) * iit_body(2, 1);
    auto t62 = transform(2, 0) * iit_body(0, 2) +
        transform(2, 1) * iit_body(1, 2) +
        transform(2, 2) * iit_body(2, 2);

    Matrix3 iit_world;

    iit_world.values[0] = t4 * transform(0, 0) +
        t9 * transform(0, 1) +
        t14 * transform(0, 2);
    iit_world.values[1] = t4 * transform(1, 0) +
        t9 * transform(1, 1) +
        t14 * transform(1, 2);
    iit_world.values[2] = t4 * transform(2, 0) +
        t9 * transform(2, 1) +
        t14 * transform(2, 2);
    iit_world.values[3] = t28 * transform(0, 0) +
        t33 * transform(0, 1) +
        t38 * transform(0, 2);
    iit_world.values[4] = t28 * transform(1, 0) +
        t33 * transform(1, 1) +
        t38 * transform(1, 2);
    iit_world.values[5] = t28 * transform(2, 0) +
        t33 * transform(2, 1) +
        t38 * transform(2, 2);
    iit_world.values[6] = t52*transform(0, 0) +
        t57 * transform(0, 1) +
        t62 * transform(0, 2);
    iit_world.values[7] = t52 * transform(1, 0) +
        t57 * transform(1, 1) +
        t62 * transform(1, 2);
    iit_world.values[8] = t52 * transform(2, 0) +
        t57 * transform(2, 1) +
        t62 * transform(2, 2);

    return iit_world;
}

RigidBody::RigidBody(std::shared_ptr<PhysicsShape> shape, PhysicsMaterial material)
    : EntityControl(fbom::FBOMObjectType("RIGID_BODY_CONTROL"), 60.0),
      m_shape(shape),
      m_material(material),
      m_awake(true),
      m_render_debug_aabb(false),
      m_aabb_renderer(new BoundingBoxRenderer()),
      m_aabb_debug_node(new Entity("physics_aabb_debug")),
      m_rigid_body(nullptr),
      m_motion_state(nullptr)
{
}

RigidBody::~RigidBody()
{
    if (m_rigid_body) {
        delete m_rigid_body;
    }

    if (m_motion_state) {
        delete m_motion_state;
    }
}

void RigidBody::UpdateTransform()
{
    m_orientation.Normalize();
    m_transform = CalculateTransformMatrix(m_position, m_orientation);
    m_inv_inertia_tensor_world = CalculateInverseInertiaWorldMatrix(m_inv_inertia_tensor, m_transform);

    m_shape->m_transform = m_transform;
}

void RigidBody::Integrate(double dt)
{
    if (m_awake) {
        m_last_acceleration = m_acceleration + (m_force_accum * m_material.GetInverseMass());

        m_linear_velocity += m_last_acceleration * dt;
        m_linear_velocity *= pow(m_material.GetLinearDamping(), dt);
        m_position += m_linear_velocity * dt;

        Vector3 angular_acceleration = m_torque_accum * m_inv_inertia_tensor_world;
        m_angular_velocity += angular_acceleration * dt;
        m_angular_velocity *= pow(m_material.GetAngularDamping(), dt);
        m_orientation += m_angular_velocity * dt;

        UpdateTransform();

        // reset accumulators
        m_force_accum = Vector3::Zero();
        m_torque_accum = Vector3::Zero();
    }
}

void RigidBody::OnAdded()
{
    if (m_render_debug_aabb) {
        m_aabb_debug_node->SetRenderable(m_aabb_renderer);
        parent->AddChild(m_aabb_debug_node);
    }

    ex_assert(m_shape != nullptr);

    ex_assert(m_rigid_body == nullptr);
    ex_assert(m_motion_state == nullptr);

    if (m_shape->m_collision_shape == nullptr) {
        throw std::invalid_argument("shape has no defined collision shape, cannot create rigid body");
    }

    bool is_dynamic = m_material.m_mass != 0.0f;

    btVector3 local_inertia(0, 0, 0);

    if (is_dynamic) {
        m_shape->m_collision_shape->calculateLocalInertia(m_material.m_mass, local_inertia);
    }

    btTransform bt_transform;
    bt_transform.setIdentity();
    bt_transform.setOrigin(ToBulletVector(parent->GetGlobalTranslation()));
    bt_transform.setRotation(ToBulletQuaternion(parent->GetGlobalRotation()));

    m_motion_state = new btDefaultMotionState(bt_transform);
    btRigidBody::btRigidBodyConstructionInfo info(m_material.m_mass, m_motion_state, m_shape->m_collision_shape, local_inertia);

    m_rigid_body = new btRigidBody(info);

    PhysicsManager::GetInstance()->RegisterBody(this);
}

void RigidBody::OnRemoved()
{
    if (m_render_debug_aabb) {
        parent->RemoveChild(m_aabb_debug_node);
    }

    ex_assert(m_rigid_body != nullptr);
    ex_assert(m_motion_state != nullptr);

    PhysicsManager::GetInstance()->UnregisterBody(this);

    delete m_rigid_body;
    m_rigid_body = nullptr;

    delete m_motion_state;
    m_motion_state = nullptr;
}

void RigidBody::OnUpdate(double dt)
{
    /*Quaternion tmp(m_orientation);
    // tmp.Invert();

    parent->SetGlobalRotation(tmp);
    parent->SetGlobalTranslation(m_position);

    m_bounding_box = m_shape->GetBoundingBox();

    if (m_render_debug_aabb) {
        m_aabb_renderer->SetAABB(m_bounding_box);
    }*/


    btTransform bt_transform;
    m_motion_state->getWorldTransform(bt_transform);

    // Matrix4 mat;
    // bt_transform.getOpenGLMatrix(mat.values.data());
    // mat.Transpose();

    // Vector3 translation = MatrixUtil::ExtractTranslation(mat);
    // // std::cout << "translation : " << translation << "\n";
    parent->SetGlobalTranslation(FromBulletVector(bt_transform.getOrigin()));
    parent->SetGlobalRotation(FromBulletQuaternion(bt_transform.getRotation()));
}

std::shared_ptr<EntityControl> RigidBody::CloneImpl()
{
    auto clone = std::make_shared<RigidBody>(m_shape, m_material);

    // TODO

    return clone;
}
} // namespace physics
} // namespace hyperion
