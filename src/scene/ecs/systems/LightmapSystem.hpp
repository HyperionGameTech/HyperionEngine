/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_LIGHTMAP_SYSTEM_HPP
#define HYPERION_ECS_LIGHTMAP_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/LightmapVolumeComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/EntityTag.hpp>

namespace hyperion {

// Assigns a MeshComponent with a lightmap volume UUID to a proper LightmapVolume.

HYP_CLASS(NoScriptBindings)
class LightmapSystem : public SystemBase
{
    HYP_OBJECT_BODY(LightmapSystem);

public:
    LightmapSystem(EntityManager& entity_manager)
        : SystemBase(entity_manager)
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
            ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},

            ComponentDescriptor<LightmapVolumeComponent, COMPONENT_RW_FLAGS_READ_WRITE, false> {},

            ComponentDescriptor<EntityTagComponent<EntityTag::LIGHTMAP_ELEMENT>, COMPONENT_RW_FLAGS_READ_WRITE, false> {}
        };
    }

    bool AssignLightmapVolume(MeshComponent& mesh_component);
};

} // namespace hyperion

#endif