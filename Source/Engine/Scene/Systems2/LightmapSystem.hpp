/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/System.hpp>
#include <Scene/components/LightmapElementComponent.hpp>
#include <Scene/components/BoundingBoxComponent.hpp>
#include <Scene/EntityTag.hpp>

namespace Hyperion {

class LightmapVolume;

HYP_CLASS(NoScriptBindings, Serialize=false)
class LightmapSystem : public SystemBase
{
    HYP_OBJECT_BODY(LightmapSystem);

public:
    virtual ~LightmapSystem() override = default;

    virtual bool AllowUpdate() const override
    {
        // Process() does nothing currently.
        return false;
    }

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
