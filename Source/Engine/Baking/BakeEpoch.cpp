/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/BakeEpoch.hpp>
#include <Baking/BakeLayer.hpp>

#include <Scene/Scene.hpp>
#include <Scene/EntityTag.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Scene/LightmapVolume.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/FogVolume.hpp>

namespace Hyperion {

namespace Baking {
namespace BakeEpoch {

void ComputeSceneHashes(const Scene& scene, BakeLayerHashes& inOutResult)
{
    const bool hasOctree = (scene.GetSceneFlags() & SceneFlags::HAS_OCTREE);
    
    if (hasOctree)
    {
        const uint64 checksum = scene.GetOctree().GetEntryListHash<EntityTag::MobStatic>().Value();

        if (checksum == inOutResult.checksum)
        {
            // use cached!!! No need to waste a recalc
            return;
        }

        inOutResult.checksum = checksum;
    }
    else
    {
        inOutResult.checksum = 0;
    }

    auto updateHashForComponent = [&]<class ComponentType>(TypeWrapper<ComponentType>, uint64& hashUUID, uint64& hashTransform)
    {
        HashCode hcUUID;
        HashCode hcTransform;

        if (scene.GetEntityManager().IsValid())
        {
            for (auto [entity, _0, _1] : scene.GetEntityManager()->GetEntitySet<ComponentType, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
            {
                hcUUID.Add(entity->GetUUID());
            }

            for (auto [entity, _0, boundingBoxComponent, _1] : scene.GetEntityManager()->GetEntitySet<ComponentType, BoundingBoxComponent, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
            {
                hcTransform.Add(entity->GetUUID());
                hcTransform.Add(boundingBoxComponent.worldAabb);
            }
        }

        hashUUID = hcUUID.Value();
        hashTransform = hcTransform.Value();
    };

    updateHashForComponent(
        TypeWrapper<MeshComponent>(),
        inOutResult.uuidHashes[BakeLayerHashes::StaticMeshEntities],
        inOutResult.transformHashes[BakeLayerHashes::StaticMeshEntities]);

    updateHashForComponent(
        TypeWrapper<TagComponent<EntityTag::Light>>(),
        inOutResult.uuidHashes[BakeLayerHashes::StaticLights],
        inOutResult.transformHashes[BakeLayerHashes::StaticLights]);
}

uint64 ComputeEpoch(const LightmapVolume& volume, BakeLayer& bakeLayer)
{
    Scene* scene = volume.GetScene();

    if (!scene)
    {
        return 0;
    }

    BakeLayerHashes& hashes = bakeLayer.sceneHashes[scene->GetUUID()];
    ComputeSceneHashes(*scene, hashes);

    return HashCode(hashes.transformHashes[BakeLayerHashes::StaticMeshEntities])
        .Combine(hashes.transformHashes[BakeLayerHashes::StaticLights])
        .Value();
}

uint64 ComputeEpoch(const EnvProbe& probe, BakeLayer& bakeLayer)
{
    Scene* scene = probe.GetScene();

    if (!scene)
    {
        return 0;
    }

    
    BakeLayerHashes& hashes = bakeLayer.sceneHashes[scene->GetUUID()];
    ComputeSceneHashes(*scene, hashes);

    // Lightmap revs affect env probes, as they are sampled when building probes
    return HashCode(hashes.transformHashes[BakeLayerHashes::StaticMeshEntities])
        .Combine(hashes.transformHashes[BakeLayerHashes::StaticLights])
        .Combine(bakeLayer.GetEpochRev(BakeLayerCategory::Lightmap))
        .Value();
}

uint64 ComputeEpoch(const FogVolume& volume, BakeLayer& bakeLayer)
{
    Scene* scene = volume.GetScene();

    if (!scene)
    {
        return 0;
    }

    BakeLayerHashes& hashes = bakeLayer.sceneHashes[scene->GetUUID()];
    ComputeSceneHashes(*scene, hashes);

    return HashCode(hashes.transformHashes[BakeLayerHashes::StaticMeshEntities])
        .Combine(hashes.transformHashes[BakeLayerHashes::StaticLights])
        .Value();
}

} // namespace BakeEpoch

} // namespace Baking

} // namespace Hyperion
