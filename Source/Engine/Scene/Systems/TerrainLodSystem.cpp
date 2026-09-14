/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Systems/TerrainLodSystem.hpp>

#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Entity.hpp>
#include <Scene/World.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Containers/Array.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>

#include <TerrainLodSystem.generated.inl>

namespace Hyperion {

static void CollectLodViewpoints(Array<Vec3f, SceneTempAllocator>& outPositions)
{
    const Handle<EngineDriver>& engineDriver = EngineDriver::GetInstance();

    if (!engineDriver)
    {
        return;
    }

    for (const Handle<World>& world : engineDriver->GetWorlds())
    {
        if (!world)
        {
            continue;
        }

        for (const Handle<Scene>& scene : world->GetScenes())
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
}

void TerrainLodSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    if (EngineGlobals::IsHeadless())
    {
        return;
    }

    Array<Vec3f, SceneTempAllocator> viewpoints;
    CollectLodViewpoints(viewpoints);

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

            const uint8 lodCount = layer->GetEffectiveLodCount();

            if (lodCount <= 1)
            {
                continue;
            }

            const BoundingBox& worldAabb = boundingBoxComponent.worldAabb;
            const uint8 currentLod = MathUtil::Min<uint8>(meshComponent.lodIndex, lodCount - 1);

            constexpr float Padding = 1.05f;

            uint32 lodVoteSum = 0; // numerator
            uint32 numVotes = 0;   // denominator

            float nearestDistance = MathUtil::Infinity<float>();
            Vec3f nearestViewpoint = Vec3f::Zero();

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

                uint8 lodForViewpoint = lodCount - 1;

                for (uint8 lodIndex = 0; lodIndex + 1 < lodCount; lodIndex++)
                {
                    float range = layer->GetLodRange(lodIndex);

                    if (lodIndex >= currentLod)
                    {
                        range *= Padding;
                    }

                    if (distance < range)
                    {
                        lodForViewpoint = lodIndex;

                        break;
                    }
                }

                if (lodForViewpoint == lodCount - 1)
                {
                    continue;
                }

                lodVoteSum += lodForViewpoint;
                numVotes++;
            }

            uint8 targetLod;

            if (numVotes == 0)
            {
                targetLod = lodCount - 1;

                if (viewpoints.Empty())
                {
                    nearestViewpoint = worldAabb.GetCenter();
                }
            }
            else
            {
                const uint32 avgLod = (lodVoteSum * 2 + numVotes) / (numVotes * 2);

                targetLod = MathUtil::Clamp<uint8>(uint8(avgLod), 0, lodCount - 1);
            }

            const float morphStart = layer->GetLodMorphStart(targetLod);
            const float morphEnd = layer->GetLodRange(targetLod);

            // the origin moves with the camera even when the LOD doesn't change, and the shader only sees it once
            // the render proxy is refreshed
            constexpr float originEpsilonSquared = 0.01f;

            const bool morphChanged = terrainCellComponent.lodMorphStart != morphStart
                || terrainCellComponent.lodMorphEnd != morphEnd
                || terrainCellComponent.lodMorphOrigin.DistanceSquared(nearestViewpoint) > originEpsilonSquared;

            if (morphChanged)
            {
                terrainCellComponent.lodMorphStart = morphStart;
                terrainCellComponent.lodMorphEnd = morphEnd;
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
