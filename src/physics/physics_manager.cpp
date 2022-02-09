#include "physics_manager.h"
#include "../rendering/environment.h"

#include "btBulletDynamicsCommon.h"

#include <algorithm>

namespace hyperion {
PhysicsManager *PhysicsManager::instance = nullptr;

PhysicsManager *PhysicsManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new PhysicsManager();
    }
    return instance;
}

PhysicsManager::PhysicsManager()
{
    m_collision_configuration = new btDefaultCollisionConfiguration();
    m_dispatcher = new btCollisionDispatcher(m_collision_configuration);
    m_broadphase_interface = new btDbvtBroadphase();
    m_solver = new btSequentialImpulseConstraintSolver();
    m_dynamics_world = new btDiscreteDynamicsWorld(
        m_dispatcher,
        m_broadphase_interface,
        m_solver,
        m_collision_configuration
    );

    m_dynamics_world->setGravity(btVector3(
        Environment::GetInstance()->GetGravity().x,
        Environment::GetInstance()->GetGravity().y,
        Environment::GetInstance()->GetGravity().z
    ));
}

PhysicsManager::~PhysicsManager()
{
    // copy, as OnRemoved() may erase rigidbodies from m_bodies.
    std::vector<physics::RigidBody*> bodies(m_bodies);

    for (physics::RigidBody *body : bodies) {
        body->OnRemoved(); // do not delete here, as it is used by shared_ptrs as entity controls
    }

    delete m_dynamics_world;
    delete m_solver;
    delete m_broadphase_interface;
    delete m_dispatcher;
    delete m_collision_configuration;
}

void PhysicsManager::RegisterBody(physics::RigidBody *body)
{
    m_dynamics_world->addRigidBody(body->m_rigid_body);

    m_bodies.push_back(body);
}

void PhysicsManager::UnregisterBody(physics::RigidBody *body)
{
    auto it = std::find(m_bodies.begin(), m_bodies.end(), body);

    if (it != m_bodies.end()) {
        m_bodies.erase(it);
    }

    if (body->m_rigid_body != nullptr) {
        m_dynamics_world->removeRigidBody(body->m_rigid_body);
    }
}

void PhysicsManager::RunPhysics(double dt)
{
    m_dynamics_world->stepSimulation(dt);
}
} // namespace hyperion
