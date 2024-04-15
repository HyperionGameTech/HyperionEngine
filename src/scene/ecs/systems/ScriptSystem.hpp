/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_ECS_SCRIPT_SYSTEM_HPP
#define HYPERION_V2_ECS_SCRIPT_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/ScriptComponent.hpp>

namespace hyperion::v2 {

class ScriptSystem : public System<
    ComponentDescriptor<ScriptComponent, COMPONENT_RW_FLAGS_READ_WRITE>
>
{
public:
    virtual ~ScriptSystem() override = default;

    // This system does not support parallel execution because scripts may modify
    // any component in the entity manager
    virtual bool AllowParallelExecution() const override
        { return false; }

    virtual void OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity) override;
    virtual void OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity) override;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif