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

HYP_CLASS(NoScriptBindings, Serialize = false)
class LightmapSystem final : public SystemBase
{
    HYP_OBJECT_BODY(LightmapSystem);

public:
    ~LightmapSystem() override = default;

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            // writes to entities with these components
            ComponentDescriptor<LightmapElementComponent, ComponentAccess::READ_WRITE> {},

            // we update probe-based lighting for dynamic entities at Process() time
            //
            // NOTE: We use ComponentAccess::READ even though we do update the SH data,
            // but nothing else modifies this data during process time and we want to avoid unnecessary System ordering changes.
            ComponentDescriptor<TagComponent<EntityTag::MobDynamic>, ComponentAccess::READ, false> {},
            
            ComponentDescriptor<TagComponent<EntityTag::UpdateSphericalHarmonicsData>, ComponentAccess::READ_WRITE, false> {},

            // used to assign entities to LightmapVolumes
            ComponentDescriptor<BoundingBoxComponent, ComponentAccess::READ> {},
            ComponentDescriptor<EntityType<LightmapVolume>, ComponentAccess::READ, false> {},

            // For irradiance probes, computing + assigning SH data to LightmapElementComponents
            ComponentDescriptor<EntityType<ProbeVolume>, ComponentAccess::READ, false> {}
        };
    }

    bool AssignLightmapVolume(
        Scene* scene,
        LightmapElementComponent& lightmapElementComponent,
        BoundingBoxComponent& boundingBoxComponent);
};

} // namespace Hyperion
