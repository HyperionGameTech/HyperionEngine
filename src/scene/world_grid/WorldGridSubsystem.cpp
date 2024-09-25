/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/world_grid/WorldGridSubsystem.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <scene/Scene.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/WorldGridComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/Logger.hpp>

#include <math/MathUtil.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion {

#pragma region WorldGridSubsystem

WorldGridSubsystem::WorldGridSubsystem()
{
}

WorldGridSubsystem::~WorldGridSubsystem()
{
}

WorldGrid *WorldGridSubsystem::GetWorldGrid(ID<Scene> scene_id)
{
    HYP_SCOPE;

    for (const UniquePtr<WorldGrid> &world_grid : m_world_grids) {
        if (world_grid->GetScene().GetID() == scene_id) {
            return world_grid.Get();
        }
    }

    return nullptr;
}

void WorldGridSubsystem::Initialize()
{
    HYP_SCOPE;
}

void WorldGridSubsystem::Shutdown()
{
    HYP_SCOPE;

    for (UniquePtr<WorldGrid> &world_grid : m_world_grids) {
        world_grid->Shutdown();
    }

    m_world_grids.Clear();
}

void WorldGridSubsystem::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME | ThreadName::THREAD_TASK);

    for (const UniquePtr<WorldGrid> &world_grid : m_world_grids) {
        world_grid->Update(delta);
    }
}

void WorldGridSubsystem::OnSceneAttached(const Handle<Scene> &scene)
{
    HYP_SCOPE;

    if (scene->IsWorldScene()) {
        UniquePtr<WorldGrid> &world_grid = m_world_grids.EmplaceBack(new WorldGrid(
            WorldGridParams { },
            scene
        ));

        world_grid->Initialize();
    }
}

void WorldGridSubsystem::OnSceneDetached(const Handle<Scene> &scene)
{
    HYP_SCOPE;

    if (scene->IsWorldScene()) {
        auto it = m_world_grids.FindIf([&scene](const UniquePtr<WorldGrid> &world_grid)
        {
            return world_grid->GetScene() == scene;
        });

        if (it != m_world_grids.End()) {
            (*it)->Shutdown();

            m_world_grids.Erase(it);
        }
    }
}

#pragma endregion WorldGridSubsystem

} // namespace hyperion
