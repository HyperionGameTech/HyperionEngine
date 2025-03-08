/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_LIGHTMAP_SYSTEM_HPP
#define HYPERION_ECS_LIGHTMAP_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/LightmapVolumeComponent.hpp>
#include <scene/ecs/components/LightmapElementComponent.hpp>

namespace hyperion {

// Assigns LightmapElementComponent a proper LightmapVolume.
class LightmapSystem : public System<
    LightmapSystem,
    ComponentDescriptor<LightmapElementComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<LightmapVolumeComponent, COMPONENT_RW_FLAGS_READ_WRITE, false>
>
{
public:
    LightmapSystem(EntityManager &entity_manager)
        : System(entity_manager)
    {
    }

    virtual ~LightmapSystem() override = default;

    virtual void OnEntityAdded(const Handle<Entity> &entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;

private:
    bool AssignVolumeToLightmapElement(LightmapElementComponent &lightmap_element_component);
};

} // namespace hyperion

#endif