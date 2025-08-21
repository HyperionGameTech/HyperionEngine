/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

class LightmapVolume;

HYP_CLASS(NoScriptBindings, Description = "Associates an Entity with a MeshComponent with the assigned LightmapVolume, if applicable.")
class LightmapSystem : public SystemBase
{
    HYP_OBJECT_BODY(LightmapSystem);

public:
    LightmapSystem(EntityManager& entityManager)
        : SystemBase(entityManager)
    {
    }

    virtual ~LightmapSystem() override = default;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            // writes to entities with these components
            ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},
            ComponentDescriptor<EntityTagComponent<EntityTag::LIGHTMAP_ELEMENT>, COMPONENT_RW_FLAGS_READ_WRITE, false> {},

            // used to assign entities to LightmapVolumes
            ComponentDescriptor<EntityType<LightmapVolume>, COMPONENT_RW_FLAGS_READ, false> {}
        };
    }

    bool AssignLightmapVolume(MeshComponent& meshComponent);
};

} // namespace hyperion

