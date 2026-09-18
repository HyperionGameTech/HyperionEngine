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
#include <Scene/View.hpp>
#include <Scene/LOD.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>
#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainStreamingCell.hpp>

#include <Core/Containers/Array.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/GameState.hpp>

#include <TerrainLodSystem.generated.inl>

namespace Hyperion {

void TerrainLodSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    if (EngineGlobals::IsHeadless() || !GetWorld())
    {
        return;
    }

    Array<LODViewData, SceneTempAllocator> viewDatas;
    GetWorld()->CollectLODViewDatas(viewDatas);

    // positions
    Array<Vec3f, SceneTempAllocator> viewpoints;
    viewpoints.Resize(viewDatas.Size());

    for (size_t i = 0; i < viewDatas.Size(); i++)
    {
        viewpoints[i] = viewDatas[i].position;
    }

    const Span<const Vec3f> viewpointsView = viewpoints.ToSpan();

    if (const Handle<WorldGrid>& worldGrid = GetWorld()->GetWorldGrid(); worldGrid.IsValid())
    {
        for (const Handle<WorldGridLayer>& layer : worldGrid->GetLayers())
        {
            if (const Handle<TerrainWorldGridLayer>& terrainLayer = DynamicCast<TerrainWorldGridLayer>(layer); terrainLayer.IsValid())
            {
                terrainLayer->SetLodViewpoints(viewpointsView);
                terrainLayer->UpdateLodSelection(viewpointsView);
            }
        }
    }

    for (Scene* scene : scenes)
    {
        bool anyDrawnMeshChanged = false;

        for (auto [entity, meshComponent, terrainPatchComponent] :
            scene->GetEntityManager()->GetEntitySet<MeshComponent, TerrainPatchComponent>().GetScopedView(GetComponentInfos()))
        {
            Handle<TerrainStreamingCell> cell = terrainPatchComponent.cell.Lock();

            if (!cell.IsValid())
            {
                continue;
            }

            bool drawnMeshChanged = false;

            if (cell->ApplyPatchLod(meshComponent, terrainPatchComponent, drawnMeshChanged))
            {
                entity->SetNeedsRenderProxyUpdate();
            }

            anyDrawnMeshChanged |= drawnMeshChanged;
        }

        if (anyDrawnMeshChanged)
        {
            scene->MarkStaticRenderResourcesChanged();
        }
    }
}

} // namespace Hyperion
