/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/systems/LightmapSystem.hpp>

#include <scene/EntityManager.hpp>
#include <scene/Scene.hpp>
#include <scene/Entity.hpp>
#include <scene/LightmapVolume.hpp>

#include <LightmapSystem.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Lightmap);

void LightmapSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    LightmapElementComponent& lightmapElementComponent = entity->GetEntityManager()->GetComponent<LightmapElementComponent>(entity);
    BoundingBoxComponent& boundingBoxComponent = entity->GetEntityManager()->GetComponent<BoundingBoxComponent>(entity);

    if (!lightmapElementComponent.lightmapVolume.IsValid())
    {
        if (!AssignLightmapVolume(entity->GetScene(), lightmapElementComponent, boundingBoxComponent))
        {
            HYP_LOG(Lightmap, Warning, "LightmapElementComponent for Entity {} could not be associated at runtime",
                entity->GetName());

            return;
        }
    }
}

void LightmapSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void LightmapSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
}

bool LightmapSystem::AssignLightmapVolume(
    Scene* scene,
    LightmapElementComponent& lightmapElementComponent,
    BoundingBoxComponent& boundingBoxComponent)
{
    for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<LightmapVolume>>().GetScopedView(GetComponentInfos()))
    {
        LightmapVolume* lightmapVolume = ObjCast<LightmapVolume>(entity);
        Assert(lightmapVolume != nullptr);

        if (boundingBoxComponent.worldAabb.Overlaps(lightmapVolume->GetWorldBounds())
            && lightmapElementComponent.lightmapVolume.GetUnsafe() != lightmapVolume)
        {
            const LightmapElement* lightmapElement = lightmapVolume->GetElement(lightmapElementComponent.lightmapElementId);

            if (!lightmapElement)
            {
                return false;
            }

            lightmapElementComponent.lightmapVolume = MakeWeakRef(lightmapVolume);

            entity->SetNeedsRenderProxyUpdate();

            return true;
        }
    }

    return false;
}

} // namespace Hyperion
