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

namespace Hyperion {

namespace Baking {
namespace BakeEpoch {

void ComputeSceneHashes(const Scene& scene, BakeLayerHashes& inOutResult)
{
    const bool hasOctree = (scene.GetSceneFlags() & SceneFlags::HAS_OCTREE);
    
    if (hasOctree)
    {
        const HashCode staticEntitiesHash = scene.GetOctree().GetEntryListHash<EntityTag::MobStatic>();

        if (staticEntitiesHash == inOutResult.staticEntitiesHash)
        {
            // use cached
            return;
        }

        inOutResult.staticEntitiesHash = staticEntitiesHash;
    }
    else
    {
        inOutResult.staticEntitiesHash = {};
    }

    auto updateHashForComponent = [&]<class ComponentType>(TypeWrapper<ComponentType>, HashCode& hashCode)
    {
        HashCode hc;

        if (scene.GetEntityManager().IsValid())
        {
            for (auto [entity, _0, boundingBoxComponent, _1] : scene.GetEntityManager()->GetEntitySet<ComponentType, BoundingBoxComponent, TagComponent<EntityTag::MobStatic>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
            {
                hc.Add(entity->GetUUID());
                hc.Add(boundingBoxComponent.worldAabb);
            }
        }

        hashCode = hc;
    };

    updateHashForComponent(TypeWrapper<MeshComponent>(), inOutResult.staticMeshEntitiesHash);
    updateHashForComponent(TypeWrapper<TagComponent<EntityTag::Light>>(), inOutResult.staticLightsHash);
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

    return hashes.staticMeshEntitiesHash
        .Combine(hashes.staticLightsHash)
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
    return hashes.staticMeshEntitiesHash
        .Combine(hashes.staticLightsHash)
        .Combine(bakeLayer.GetEpochRev(BakeLayerCategory::Lightmap))
        .Value();
}

} // namespace BakeEpoch

} // namespace Baking

} // namespace Hyperion
