/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_ECS_UI_SYSTEM_HPP
#define HYPERION_V2_ECS_UI_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>

namespace hyperion::v2 {

class UISystem : public System<
    ComponentDescriptor<UIComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>
>
{
public:
    virtual ~UISystem() override = default;

    virtual void OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity) override;
    virtual void OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity) override;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif