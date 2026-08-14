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

#include <Core/Containers/Set.hpp>

namespace Hyperion {

class LightmapVolume;

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

    void MarkLightmapVolumeIdUsed(LightmapVolumeId id)
    {
        if (id == Invalid<LightmapVolumeId>)
        {
            return;
        }

        m_freedLightmapVolumeIds.Erase(uint32(id));
    }

    void MarkLightmapVolumeIdFreed(LightmapVolumeId id)
    {
        if (id == Invalid<LightmapVolumeId>)
        {
            return;
        }

        if (m_freedLightmapVolumeIds.Contains(uint32(id)))
        {
            return;
        }

        m_freedLightmapVolumeIds.PushBack(uint32(id));
    }

    bool IsIdForAliveLightmapVolume(LightmapVolumeId id) const
    {
        return id != Invalid<LightmapVolumeId>
            && !m_freedLightmapVolumeIds.Contains(uint32(id));
    }

private:
    void OnAddedToWorld(World* world) override;

    SystemComponentDescriptors GetComponentDescriptors() const override
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
        Scene& scene,
        Entity& srcEntity,
        LightmapElementComponent& lightmapElementComponent,
        BoundingBoxComponent& boundingBoxComponent);

    HYP_FIELD(Property = "NextLightmapVolumeId", Serialize)
    uint32 m_nextLightmapVolumeId;

    HYP_FIELD(Property = "FreedLightmapVolumeIds", Serialize)
    Array<uint32> m_freedLightmapVolumeIds;
};

} // namespace Hyperion
