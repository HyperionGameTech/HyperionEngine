/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Systems/TerrainLodSystem.hpp>

#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/EntityTag.hpp>
#include <Scene/Entity.hpp>
#include <Scene/World.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>

#include <Rendering/Mesh.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Containers/Array.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/GameState.hpp>

#include <TerrainLodSystem.generated.inl>

namespace Hyperion {

template <EntityTag CameraTag>
static bool FindTaggedCameraPosition(const World& world, bool foregroundScenesOnly, Vec3f& outPosition)
{
    for (const Handle<Scene>& scene : world.GetScenes())
    {
        if (!scene || (foregroundScenesOnly && !(scene->GetSceneFlags() & SceneFlags::FOREGROUND)))
        {
            continue;
        }

        EntityManager* entityManager = scene->GetEntityManager();

        if (!entityManager)
        {
            continue;
        }

        for (auto [camera, _] : entityManager->GetEntitySet<EntityType<Camera>, TagComponent<CameraTag>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            outPosition = camera->GetWorldTranslation();

            return true;
        }
    }

    return false;
}

// Only the camera actually being looked through drives LOD. Every other camera with a streaming volume (e.g. the player
// camera sitting in the scene while editing) would otherwise claim the cells nearest to it, and a cell pops whenever its
// nearest camera changes.
static void CollectLodViewpoints(const World& world, Array<Vec3f, SceneTempAllocator>& outPositions)
{
    Vec3f cameraPosition;

    if (world.GetGameState().IsStopped() && FindTaggedCameraPosition<EntityTag::EditorCamera>(world, false, cameraPosition))
    {
        outPositions.PushBack(cameraPosition);

        return;
    }

    if (FindTaggedCameraPosition<EntityTag::PrimaryCamera>(world, true, cameraPosition))
    {
        outPositions.PushBack(cameraPosition);

        return;
    }

    for (const Handle<Scene>& scene : world.GetScenes())
    {
        if (!scene)
        {
            continue;
        }

        EntityManager* entityManager = scene->GetEntityManager();

        if (!entityManager)
        {
            continue;
        }

        for (auto [camera] : entityManager->GetEntitySet<EntityType<Camera>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!(camera->GetCameraFlags() & CameraFlags::HasStreamingVolume))
            {
                continue;
            }

            outPositions.PushBack(camera->GetWorldTranslation());
        }
    }
}

// Refining early is always seamless since a finer mesh can morph all the way to the coarser surface, but coarsening is
// only seamless once the whole cell is past the current LOD's range - so the two directions use different thresholds.
static uint8 SelectTerrainLod(const TerrainWorldGridLayer& layer, uint8 lodCount, uint8 currentLod, float nearestDistance)
{
    constexpr float RefineRangeScale = 1.05f;
    constexpr float CoarsenRangeScale = 1.1f;

    for (uint8 lodIndex = 0; lodIndex + 1 < lodCount; lodIndex++)
    {
        const float rangeScale = lodIndex < currentLod ? RefineRangeScale : CoarsenRangeScale;

        if (nearestDistance < layer.GetLodRange(lodIndex) * rangeScale)
        {
            return lodIndex;
        }
    }

    return lodCount - 1;
}

void TerrainLodSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    if (EngineGlobals::IsHeadless() || !GetWorld())
    {
        return;
    }

    Array<Vec3f, SceneTempAllocator> viewpoints;
    CollectLodViewpoints(*GetWorld(), viewpoints);

    for (Scene* scene : scenes)
    {
        for (auto [entity, meshComponent, terrainCellComponent, boundingBoxComponent] :
            scene->GetEntityManager()->GetEntitySet<MeshComponent, TerrainCellComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos()))
        {
            Handle<TerrainWorldGridLayer> layer = terrainCellComponent.layer.Lock();

            if (!layer.IsValid())
            {
                continue;
            }

            if (!meshComponent.mesh)
            {
                continue;
            }

            // the layer's LOD settings can change after this cell's mesh was built - bands must match the LODs it actually has
            const uint8 lodCount = meshComponent.mesh->GetMeshDesc().GetNumLods();

            if (lodCount <= 1)
            {
                continue;
            }

            const BoundingBox& worldAabb = boundingBoxComponent.worldAabb;

            // LOD selection and the shader's per-vertex morph must be driven by the same viewpoint, otherwise a switch
            // happens while vertices are still mid-morph
            float nearestDistance = MathUtil::Infinity<float>();
            Vec3f nearestViewpoint = worldAabb.GetCenter();

            for (const Vec3f& viewpoint : viewpoints)
            {
                const Vec3f closestPoint {
                    MathUtil::Clamp(viewpoint.x, worldAabb.min.x, worldAabb.max.x),
                    MathUtil::Clamp(viewpoint.y, worldAabb.min.y, worldAabb.max.y),
                    MathUtil::Clamp(viewpoint.z, worldAabb.min.z, worldAabb.max.z)
                };

                const float distance = (viewpoint - closestPoint).Length();

                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    nearestViewpoint = viewpoint;
                }
            }

            const uint8 currentLod = MathUtil::Min<uint8>(meshComponent.lodIndex, lodCount - 1);
            const uint8 targetLod = SelectTerrainLod(*layer, lodCount, currentLod, nearestDistance);

            const float morphStart = layer->GetLodMorphStart(targetLod);
            const float morphEnd = layer->GetLodRange(targetLod);
            const float rangeMultiplier = layer->GetEffectiveLodRangeMultiplier();

            // the origin moves with the camera even when the LOD doesn't change, and the shader only sees it once
            // the render proxy is refreshed
            constexpr float originEpsilonSquared = 0.01f;

            const bool morphChanged = terrainCellComponent.lodMorphStart != morphStart
                || terrainCellComponent.lodMorphEnd != morphEnd
                || terrainCellComponent.lodRangeMultiplier != rangeMultiplier
                || terrainCellComponent.lodMorphOrigin.DistanceSquared(nearestViewpoint) > originEpsilonSquared;

            if (morphChanged)
            {
                terrainCellComponent.lodMorphStart = morphStart;
                terrainCellComponent.lodMorphEnd = morphEnd;
                terrainCellComponent.lodRangeMultiplier = rangeMultiplier;
                terrainCellComponent.lodMorphOrigin = nearestViewpoint;
            }

            if (meshComponent.lodIndex != targetLod || morphChanged)
            {
                meshComponent.lodIndex = targetLod;

                entity->SetNeedsRenderProxyUpdate();
            }
        }
    }
}

} // namespace Hyperion
