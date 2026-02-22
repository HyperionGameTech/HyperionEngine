/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/components/LightmapElementComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/EntityTag.hpp>

namespace Hyperion {

class LightmapVolume;

HYP_CLASS(NoScriptBindings, Serialize=false)
class LightmapSystem : public SystemBase
{
    HYP_OBJECT_BODY(LightmapSystem);

public:
    virtual ~LightmapSystem() override = default;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            // writes to entities with these components
            ComponentDescriptor<LightmapElementComponent, ComponentAccess::READ_WRITE> {},

            // used to assign entities to LightmapVolumes
            ComponentDescriptor<BoundingBoxComponent, ComponentAccess::READ> {},
            ComponentDescriptor<EntityType<LightmapVolume>, ComponentAccess::READ, false> {}
        };
    }

    bool AssignLightmapVolume(
        Scene* scene,
        LightmapElementComponent& lightmapElementComponent,
        BoundingBoxComponent& boundingBoxComponent);
};

} // namespace Hyperion
