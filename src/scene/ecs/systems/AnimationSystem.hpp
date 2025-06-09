/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ANIMATION_SYSTEM_HPP
#define HYPERION_ECS_ANIMATION_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/AnimationComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class AnimationSystem : public SystemBase
{
    HYP_OBJECT_BODY(AnimationSystem);

public:
    AnimationSystem(EntityManager& entity_manager)
        : SystemBase(entity_manager)
    {
    }

    virtual ~AnimationSystem() override = default;

    virtual void OnEntityAdded(const Handle<Entity>& entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<AnimationComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},
            ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ> {}
        };
    }
};

} // namespace hyperion

#endif