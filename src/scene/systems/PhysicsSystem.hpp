/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/components/RigidBodyComponent.hpp>
#include <scene/components/TransformComponent.hpp>

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
    PhysicsSystem(EntityManager& entityManager)
        : SystemBase(entityManager)
    {
    }

    virtual ~PhysicsSystem() override = default;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<RigidBodyComponent, ComponentRWFlags::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentRWFlags::READ_WRITE> {}
        };
    }
};

} // namespace hyperion

