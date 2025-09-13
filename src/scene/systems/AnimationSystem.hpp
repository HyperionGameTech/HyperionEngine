/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/components/AnimationComponent.hpp>
#include <scene/components/MeshComponent.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class AnimationSystem : public SystemBase
{
    HYP_OBJECT_BODY(AnimationSystem);

public:
    AnimationSystem(EntityManager& entityManager)
        : SystemBase(entityManager)
    {
    }

    virtual ~AnimationSystem() override = default;

    virtual void OnEntityAdded(Entity* entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<AnimationComponent, ComponentRWFlags::READ_WRITE> {},
            ComponentDescriptor<MeshComponent, ComponentRWFlags::READ> {}
        };
    }
};

} // namespace hyperion

