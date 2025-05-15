/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ANIMATION_SYSTEM_HPP
#define HYPERION_ECS_ANIMATION_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/AnimationComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

namespace hyperion {

class AnimationSystem : public System<
    AnimationSystem,
    ComponentDescriptor<AnimationComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ>
>
{
public:
    AnimationSystem(EntityManager &entity_manager)
        : System(entity_manager)
    {
    }

    virtual ~AnimationSystem() override = default;

    virtual void Process(GameCounter::TickUnit delta) override;
};

} // namespace hyperion

#endif