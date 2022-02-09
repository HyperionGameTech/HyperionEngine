#ifndef APEX_PHYSICS_RIGID_BODY_H
#define APEX_PHYSICS_RIGID_BODY_H

#include "../control.h"
#include "../entity.h"
#include "physics_shape.h"
#include "physics_material.h"
#include "../math/vector3.h"
#include "../math/matrix4.h"
#include "../math/quaternion.h"
#include "../math/bounding_box.h"

#include <memory>

class btRigidBody;
class btDefaultMotionState;

namespace hyperion {
class BoundingBoxRenderer;
class PhysicsManager;
namespace physics {
class RigidBody : public EntityControl {
    friend class ::hyperion::PhysicsManager;
public:
    RigidBody(std::shared_ptr<PhysicsShape> shape, PhysicsMaterial material);
    virtual ~RigidBody() override;

    inline std::shared_ptr<PhysicsShape> GetPhysicsShape() const { return m_shape; }

    inline const PhysicsMaterial &GetPhysicsMaterial() const { return m_material; }
    inline PhysicsMaterial &GetPhysicsMaterial() { return m_material; }
    inline void SetPhysicsMaterial(const PhysicsMaterial &material) { m_material = material; }

    inline bool IsAwake() const
        { return m_awake; }
    inline void SetAwake(bool awake = true)
        { if (!(m_awake = awake)) { m_linear_velocity = 0; m_angular_velocity = 0; } }

    inline void SetInertiaTensor(const Matrix3 &inertia_tensor)
        { m_inv_inertia_tensor = inertia_tensor; m_inv_inertia_tensor.Invert(); }
    inline const Matrix3 &GetInverseInertiaTensor() const
        { return m_inv_inertia_tensor; }
    inline void SetInverseInertiaTensor(const Matrix3 &inv_inertia_tensor)
        { m_inv_inertia_tensor = inv_inertia_tensor; }
    inline const Matrix3 &GetInverseInertiaTensorWorld() const
        { return m_inv_inertia_tensor_world; }

    inline bool IsStatic() const
        { return m_material.GetInverseMass() == 0.0; }

    inline const Vector3 &GetLinearVelocity() const
        { return m_linear_velocity; }
    inline void SetLinearVelocity(const Vector3 &linear_velocity)
        { m_linear_velocity = linear_velocity; }
    inline void AddLinearVelocity(const Vector3 &linear_velocity)
        { m_linear_velocity += linear_velocity; }

    inline const Vector3 &GetAngularVelocity() const
        { return m_angular_velocity; }
    inline void SetAngularVelocity(const Vector3 &angular_velocity)
        { m_angular_velocity = angular_velocity; }
    inline void AddAngularVelocity(const Vector3 &angular_velocity)
        { m_angular_velocity += angular_velocity; }

    inline const Vector3 &GetAcceleration() const
        { return m_acceleration; }
    inline void SetAcceleration(const Vector3 &acceleration)
        { m_acceleration = acceleration; }
    inline const Vector3 &GetLastAcceleration() const
        { return m_last_acceleration; }

    inline void ApplyForce(const Vector3 &force)
        { m_force_accum += force; m_awake = true; }
    inline void ApplyTorque(const Vector3 &torque)
        { m_torque_accum += torque; m_awake = true; }

    inline const Vector3 &GetPosition() const
        { return m_position; }
    inline Vector3 &GetPosition()
        { return m_position; }
    inline void SetPosition(const Vector3 &position)
        { m_position = position; UpdateTransform(); }
    inline const Quaternion &GetOrientation() const
        { return m_orientation; }
    inline Quaternion &GetOrientation()
        { return m_orientation; }
    inline void SetOrientation(const Quaternion &orientation)
        { m_orientation = orientation; UpdateTransform(); }

    inline const BoundingBox &GetBoundingBox() const { return m_bounding_box; }

    inline void SetRenderDebugBoundingBox(bool value) { m_render_debug_aabb = value; }
    inline bool GetRenderDebugBoundingBox() const { return m_render_debug_aabb; }

    // updates the transform of the PhysicsShape within this object
    void UpdateTransform();
    // perform physics calculations on this rigidbody
    void Integrate(double dt);

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(double dt) override;

protected:
    btRigidBody *m_rigid_body;
    btDefaultMotionState *m_motion_state;

private:
    virtual std::shared_ptr<EntityControl> CloneImpl() override;

    std::shared_ptr<PhysicsShape> m_shape;
    PhysicsMaterial m_material;
    bool m_awake;
    Matrix3 m_inv_inertia_tensor;
    Matrix3 m_inv_inertia_tensor_world;
    Matrix4 m_transform;
    Vector3 m_linear_velocity;
    Vector3 m_angular_velocity;
    Vector3 m_acceleration;
    Vector3 m_last_acceleration;
    Vector3 m_force_accum;
    Vector3 m_torque_accum;
    Vector3 m_position;
    Quaternion m_orientation;
    BoundingBox m_bounding_box;
    bool m_render_debug_aabb;
    std::shared_ptr<BoundingBoxRenderer> m_aabb_renderer;
    std::shared_ptr<Entity> m_aabb_debug_node;
};
} // namespace physics
} // namespace hyperion

#endif
