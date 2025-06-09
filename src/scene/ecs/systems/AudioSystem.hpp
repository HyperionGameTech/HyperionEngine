/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_AUDIO_SYSTEM_HPP
#define HYPERION_ECS_AUDIO_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/AudioComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class AudioSystem : public SystemBase
{
    HYP_OBJECT_BODY(AudioSystem);

public:
    AudioSystem(EntityManager& entity_manager)
        : SystemBase(entity_manager)
    {
    }

    virtual ~AudioSystem() override = default;

    virtual void OnEntityAdded(const Handle<Entity>& entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<AudioComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},
            ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ> {}
        };
    }
};

} // namespace hyperion

#endif