/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_ECS_AUDIO_SYSTEM_HPP
#define HYPERION_V2_ECS_AUDIO_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/AudioComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>

namespace hyperion::v2 {

class AudioSystem : public System<
    ComponentDescriptor<AudioComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>
>
{
public:
    virtual ~AudioSystem() override = default;

    virtual void OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity) override;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif