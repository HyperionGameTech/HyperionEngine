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
        return FindVolume(id) != nullptr;
    }

    void RegisterVolume(LightmapVolume* volume);
    void UnregisterVolume(LightmapVolume* volume);

    LightmapVolume* FindVolume(LightmapVolumeId id) const;

    /*! \brief Point the entity at its owning volume if that volume is in the world and has a bake for the applied layer, otherwise clear it */
    bool ResolveVolumeForEntity(Entity& srcEntity, LightmapElementComponent& lightmapElementComponent);

    /*! \brief Re-resolve the volume lighting each entity */
    void ResolveVolumeAssignments();

    /*! \brief Give every volume a range of stencil values (one per atlas page) that doesn't collide with any volume whose
     *  lighting bounds overlap it, so LightmapPass can route each pixel to the page it was baked into  */
    void AssignStencilValues();

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

    LightmapVolume* ResolveVolume(const LightmapElementComponent& lightmapElementComponent) const;

    bool ApplyResolvedVolume(Entity& srcEntity, LightmapElementComponent& lightmapElementComponent, LightmapVolume* resolvedVolume);

    HYP_FIELD(Property = "NextLightmapVolumeId", Serialize)
    uint32 m_nextLightmapVolumeId;

    HYP_FIELD(Property = "FreedLightmapVolumeIds", Serialize)
    Array<uint32> m_freedLightmapVolumeIds;

    Array<LightmapVolume*> m_volumes;
};

} // namespace Hyperion
