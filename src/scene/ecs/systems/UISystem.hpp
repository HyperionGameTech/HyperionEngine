/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_UI_SYSTEM_HPP
#define HYPERION_ECS_UI_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>

namespace hyperion {

class UISystem : public System<
    UISystem,
    ComponentDescriptor<UIComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>
>
{
public:
    UISystem(EntityManager &entity_manager)
        : System(entity_manager)
    {
    }

    virtual ~UISystem() override = default;

    virtual void OnEntityAdded(const Handle<Entity> &entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;
};

} // namespace hyperion

#endif