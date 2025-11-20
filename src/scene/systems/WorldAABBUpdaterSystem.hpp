/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/EntityTag.hpp>

#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class WorldAABBUpdaterSystem : public SystemBase
{
    HYP_OBJECT_BODY(WorldAABBUpdaterSystem);

public:
    virtual ~WorldAABBUpdaterSystem() override = default;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<BoundingBoxComponent, ComponentRWFlags::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentRWFlags::READ> {},

            ComponentDescriptor<TagComponent<EntityTag::UPDATE_AABB>, ComponentRWFlags::READ, false> {}
        };
    }

    bool ProcessEntity(Entity* entity, BoundingBoxComponent& boundingBoxComponent, TransformComponent& transformComponent);
};

} // namespace hyperion
