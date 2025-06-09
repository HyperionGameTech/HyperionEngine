/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_PHYSICS_SYSTEM_HPP
#define HYPERION_ECS_PHYSICS_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/RigidBodyComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>

namespace hyperion {

/*! \brief System for updating transforms of objects with RigidBodyComponent to sync with physics simulation.
 *
 * This system processes entities with RigidBodyComponent and TransformComponent,
 * updating their transforms based on the physics simulation results. */
HYP_CLASS(NoScriptBindings)
class PhysicsSystem : public SystemBase
{
    HYP_OBJECT_BODY(PhysicsSystem);

public:
    PhysicsSystem(EntityManager& entity_manager)
        : SystemBase(entity_manager)
    {
    }

    virtual ~PhysicsSystem() override = default;

    virtual void OnEntityAdded(const Handle<Entity>& entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<RigidBodyComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},
            ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ_WRITE> {}
        };
    }
};

} // namespace hyperion

#endif