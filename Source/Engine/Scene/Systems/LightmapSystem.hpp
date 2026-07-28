/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>

#include <Scene/Components/LightmapElementComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>

#include <Scene/EntityTag.hpp>

namespace Hyperion {

class LightmapVolume;
class ProbeVolume;

HYP_CLASS(NoScriptBindings, Serialize)
class LightmapSystem final : public SystemBase
{
    HYP_OBJECT_BODY(LightmapSystem);

public:
    LightmapSystem();
    ~LightmapSystem() override = default;

    bool AllowUpdate() const override
    {
        return false;
    }

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    HYP_NODISCARD LightmapVolumeId AllocateLightmapVolumeId();

private:
    void OnAddedToWorld(World* world) override;

    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            // writes to entities with these components
            ComponentDescriptor<LightmapElementComponent, ComponentAccess::READ_WRITE> {},

            // used to assign entities to LightmapVolumes
            ComponentDescriptor<BoundingBoxComponent, ComponentAccess::READ> {},
            ComponentDescriptor<EntityType<LightmapVolume>, ComponentAccess::READ, false> {},

            // For irradiance probes, computing + assigning SH data to LightmapElementComponents
            ComponentDescriptor<EntityType<ProbeVolume>, ComponentAccess::READ, false> {}
        };
    }

    bool AssignLightmapVolume(
        Scene& scene,
        Entity& srcEntity,
        LightmapElementComponent& lightmapElementComponent,
        BoundingBoxComponent& boundingBoxComponent);

    HYP_FIELD(Property = "NextLightmapVolumeId", Serialize)
    uint32 m_nextLightmapVolumeId;
};

} // namespace Hyperion
