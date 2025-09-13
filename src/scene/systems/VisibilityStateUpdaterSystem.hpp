/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/EntityTag.hpp>

#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class VisibilityStateUpdaterSystem : public SystemBase
{
    HYP_OBJECT_BODY(VisibilityStateUpdaterSystem);

public:
    VisibilityStateUpdaterSystem(EntityManager& entityManager)
        : SystemBase(entityManager)
    {
    }

    virtual ~VisibilityStateUpdaterSystem() override = default;

    virtual bool ShouldCreateForScene(Scene* scene) const override;
    
    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<VisibilityStateComponent, ComponentRWFlags::READ_WRITE> {},
            ComponentDescriptor<BoundingBoxComponent, ComponentRWFlags::READ> {},

            ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_VISIBILITY_STATE>, ComponentRWFlags::READ, false> {}
        };
    }
};

} // namespace hyperion

