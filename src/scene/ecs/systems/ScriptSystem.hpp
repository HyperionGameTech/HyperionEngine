/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_SCRIPT_SYSTEM_HPP
#define HYPERION_ECS_SCRIPT_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/ScriptComponent.hpp>

#include <core/functional/Delegate.hpp>

namespace hyperion {

class ScriptSystem : public System<
    ComponentDescriptor<ScriptComponent, COMPONENT_RW_FLAGS_READ_WRITE>
>
{
public:
    ScriptSystem(EntityManager &entity_manager);
    virtual ~ScriptSystem() override = default;

    // This system does not support parallel execution because scripts may modify
    // any component in the entity manager
    virtual bool AllowParallelExecution() const override
        { return false; }

    virtual void OnEntityAdded(ID<Entity> entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;

private:
    DelegateHandlerSet  m_delegate_handlers;
};

} // namespace hyperion

#endif