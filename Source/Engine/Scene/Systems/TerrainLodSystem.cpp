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

#include <Scene/Camera/Camera.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>
#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainStreamingCell.hpp>

#include <Core/Containers/Array.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/GameState.hpp>

#include <TerrainLodSystem.generated.inl>

namespace Hyperion {

static bool IsLodView(const View& view)
{
    const EnumFlags<ViewFlags> excludedFlags = ViewFlags::SHADOW_VIEW
        | ViewFlags::ENV_PROBE_VIEW
        | ViewFlags::UI_VIEW
        | ViewFlags::BAKER_VIEW
        | ViewFlags::RAY_TRACING
        | ViewFlags::CUBEMAP_FACE_VIEW;

    return (view.GetFlags() & ViewFlags::GBUFFER)
        && !(view.GetFlags() & excludedFlags)
        && view.GetCamera() != nullptr;
}

static void CollectLodViewpoints(const World& world, Array<Vec3f, SceneTempAllocator>& outPositions)
{
    const bool preferEditorViews = world.GetGameState().IsStopped();

    for (View* view : world.GetSimThreadViews())
    {
        if (!view || !IsLodView(*view))
        {
            continue;
        }

        const bool isEditorView = bool(view->GetFlags() & ViewFlags::EDITOR_VIEW);

        if (isEditorView == preferEditorViews)
        {
            outPositions.PushBack(view->GetCamera()->GetWorldTranslation());
        }
    }

    if (outPositions.Any())
    {
        return;
    }

    for (View* view : world.GetSimThreadViews())
    {
        if (view && IsLodView(*view))
        {
            outPositions.PushBack(view->GetCamera()->GetWorldTranslation());
        }
    }

    if (outPositions.Any())
    {
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

void TerrainLodSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    if (EngineGlobals::IsHeadless() || !GetWorld())
    {
        return;
    }

    Array<Vec3f, SceneTempAllocator> viewpoints;
    CollectLodViewpoints(*GetWorld(), viewpoints);

    const Span<const Vec3f> viewpointsView(viewpoints.Data(), viewpoints.Size());

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

            if (cell->ApplyPatchLod(viewpointsView, meshComponent, terrainPatchComponent, drawnMeshChanged))
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
