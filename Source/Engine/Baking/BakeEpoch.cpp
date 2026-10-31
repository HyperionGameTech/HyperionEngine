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
#include <Scene/Light/Light.hpp>

#include <Rendering/Mesh.hpp>

namespace Hyperion {

namespace Baking {
namespace BakeEpoch {

namespace {

void ComputeStaticLightHashes(const Scene& scene, BakeLayerHashes& inOutResult)
{
    HashCode hcUUID;
    HashCode hcLighting;
    HashCode hcLocalUUID;
    HashCode hcLocalLighting;

    if (scene.GetEntityManager().IsValid())
    {
        for (auto [light, _] : scene.GetEntityManager()->GetEntitySet<EntityType<Light>, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            const HashCode lightingHash = light->GetLightingHashCode();

            hcUUID.Add(light->GetUUID());

            hcLighting.Add(light->GetUUID());
            hcLighting.Add(lightingHash);

            if (light->GetLightType() == LightType::Directional)
            {
                continue;
            }

            hcLocalUUID.Add(light->GetUUID());

            hcLocalLighting.Add(light->GetUUID());
            hcLocalLighting.Add(lightingHash);
        }
    }

    inOutResult.uuidHashes[BakeLayerHashes::StaticLights] = hcUUID.Value();
    inOutResult.transformHashes[BakeLayerHashes::StaticLights] = hcLighting.Value();

    inOutResult.uuidHashes[BakeLayerHashes::StaticLocalLights] = hcLocalUUID.Value();
    inOutResult.transformHashes[BakeLayerHashes::StaticLocalLights] = hcLocalLighting.Value();
}

void ComputeStaticMeshHashes(const Scene& scene, BakeLayerHashes& inOutResult)
{
    HashCode hcUUID;
    HashCode hcTransform;

    if (scene.GetEntityManager().IsValid())
    {
        for (auto [entity, _0, _1] : scene.GetEntityManager()->GetEntitySet<MeshComponent, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            hcUUID.Add(entity->GetUUID());
        }

        for (auto [entity, meshComponent, boundingBoxComponent, _] : scene.GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            hcTransform.Add(entity->GetUUID());
            hcTransform.Add(boundingBoxComponent.worldAabb);

            if (meshComponent.mesh.IsValid())
            {
                hcTransform.Add(meshComponent.mesh->GetUUID());
                hcTransform.Add(meshComponent.mesh->GetLod0DataRevision());
            }
        }
    }

    inOutResult.uuidHashes[BakeLayerHashes::StaticMeshEntities] = hcUUID.Value();
    inOutResult.transformHashes[BakeLayerHashes::StaticMeshEntities] = hcTransform.Value();
}

uint64 ComputeLightShadowHash(const Light& light)
{
    HashCode hc;
    hc.Add(light.GetLightType());
    hc.Add(light.GetWorldTranslation());
    hc.Add(light.GetShadowMapDimensions());

    if (light.GetLightType() != LightType::Directional)
    {
        hc.Add(light.GetRadius());
    }

    if (light.GetLightType() == LightType::Spot)
    {
        hc.Add(light.GetSpotDirection());
        hc.Add(light.GetSpotAngles());
    }

    return hc.Value();
}

} // namespace

void ComputeSceneHashes(const Scene& scene, BakeLayerHashes& inOutResult)
{
    ComputeStaticLightHashes(scene, inOutResult);

    const bool hasOctree = (scene.GetSceneFlags() & SceneFlags::HAS_OCTREE);

    if (hasOctree)
    {
        // mesh swaps (CSG) and in-place mesh edits can leave every AABB, and so the octree hash, unchanged
        const uint64 checksum = scene.GetOctree().GetEntryListHash<EntityTag::MobStatic>()
            .Combine(scene.GetStaticRenderResourcesRevision())
            .Value();

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

    ComputeStaticMeshHashes(scene, inOutResult);
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
        .Combine(hashes.transformHashes[BakeLayerHashes::StaticLocalLights])
        .Value();
}

uint64 ComputeEpoch(const Light& light, BakeLayer& bakeLayer)
{
    Scene* scene = light.GetScene();

    if (!scene)
    {
        return 0;
    }

    BakeLayerHashes& hashes = bakeLayer.sceneHashes[scene->GetUUID()];
    ComputeSceneHashes(*scene, hashes);

    const uint64 lightHash = ComputeLightShadowHash(light);

    LightShadowEpochCache& cache = bakeLayer.lightShadowEpochs[light.GetUUID()];

    const bool canReuse = (scene->GetSceneFlags() & SceneFlags::HAS_OCTREE)
        && cache.epoch != 0
        && cache.sceneChecksum == hashes.checksum
        && cache.lightHash == lightHash;

    if (canReuse)
    {
        return cache.epoch;
    }

    BoundingBox lightBounds = light.GetLightType() == LightType::Spot
        ? light.GetWorldBounds()
        : BoundingBox();

    if (!lightBounds.IsValid() || !lightBounds.IsFinite())
    {
        lightBounds = BoundingBox(light.GetBoundingSphere(true));
    }

    HashCode hcCasters { lightHash };

    if (scene->GetEntityManager().IsValid())
    {
        for (auto [entity, meshComponent, boundingBoxComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!boundingBoxComponent.worldAabb.IsValid() || !lightBounds.Overlaps(boundingBoxComponent.worldAabb))
            {
                continue;
            }

            hcCasters.Add(entity->GetUUID());
            hcCasters.Add(boundingBoxComponent.worldAabb);

            if (meshComponent.mesh.IsValid())
            {
                hcCasters.Add(meshComponent.mesh->GetUUID());
                hcCasters.Add(meshComponent.mesh->GetLod0DataRevision());
            }
        }
    }

    cache.sceneChecksum = hashes.checksum;
    cache.lightHash = lightHash;
    cache.epoch = hcCasters.Value();

    return cache.epoch;
}

} // namespace BakeEpoch

} // namespace Baking

} // namespace Hyperion
