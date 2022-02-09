#ifndef PHYSICS_MANAGER_H
#define PHYSICS_MANAGER_H

#define VELOCITY_EPSILON 0.0005
#define POSITION_EPSILON 0.0005

#include "rigid_body.h"
#include "box_physics_shape.h"
#include "sphere_physics_shape.h"
#include "plane_physics_shape.h"

#include <vector>
#include <memory>

class btDiscreteDynamicsWorld;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btSequentialImpulseConstraintSolver;

namespace hyperion {
class PhysicsManager {
    friend class physics::RigidBody;
public:
    static PhysicsManager *GetInstance();

    PhysicsManager();
    ~PhysicsManager();

    void RegisterBody(physics::RigidBody *body);
    void UnregisterBody(physics::RigidBody *body);
    void RunPhysics(double dt);

private:
    static PhysicsManager *instance;
    std::vector<physics::RigidBody*> m_bodies;

    btDiscreteDynamicsWorld *m_dynamics_world;
    btDefaultCollisionConfiguration *m_collision_configuration;
    btCollisionDispatcher *m_dispatcher;
    btBroadphaseInterface *m_broadphase_interface;
    btSequentialImpulseConstraintSolver *m_solver;
};
}

#endif
