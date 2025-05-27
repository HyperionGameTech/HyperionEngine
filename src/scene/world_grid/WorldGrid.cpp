/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/world_grid/WorldGrid.hpp>
#include <scene/world_grid/WorldGridPlugin.hpp>

#include <scene/Scene.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/WorldGridComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/math/MathUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(WorldGrid, Scene);

#pragma region Helpers

static const FixedArray<WorldGridPatchNeighbor, 8> GetPatchNeighbors(const Vec2i& coord)
{
    return {
        WorldGridPatchNeighbor { coord + Vec2i { 1, 0 } },
        WorldGridPatchNeighbor { coord + Vec2i { -1, 0 } },
        WorldGridPatchNeighbor { coord + Vec2i { 0, 1 } },
        WorldGridPatchNeighbor { coord + Vec2i { 0, -1 } },
        WorldGridPatchNeighbor { coord + Vec2i { 1, -1 } },
        WorldGridPatchNeighbor { coord + Vec2i { -1, -1 } },
        WorldGridPatchNeighbor { coord + Vec2i { 1, 1 } },
        WorldGridPatchNeighbor { coord + Vec2i { -1, 1 } }
    };
}

static Vec2i WorldSpaceToPatchCoord(const WorldGrid& world_grid, const Vec3f& world_position)
{
    Vec3f scaled = world_position - world_grid.GetParams().offset;
    scaled *= Vec3f::One() / (world_grid.GetParams().scale * (Vec3f(world_grid.GetParams().patch_size) - 1.0f));
    scaled = MathUtil::Floor(scaled);

    return Vec2i { int(scaled.x), int(scaled.z) };
}

#pragma endregion Helpers

#pragma region WorldGridPatch

WorldGridPatch::WorldGridPatch(const WorldGridPatchInfo& patch_info)
    : m_patch_info(patch_info)
{
}

WorldGridPatch::~WorldGridPatch()
{
}

#pragma endregion WorldGridPatch

#pragma region WorldGridState

void WorldGridState::PushUpdate(WorldGridPatchUpdate&& update)
{
    Mutex::Guard guard(patch_update_queue_mutex);

    patch_update_queue.Push(std::move(update));

    patch_update_queue_size.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
}

#pragma endregion WorldGridState

#pragma region WorldGrid

WorldGrid::WorldGrid()
    : m_params {},
      m_root_node(Handle<Node>(CreateObject<Node>())),
      m_is_initialized(false)
{
}

WorldGrid::WorldGrid(const WorldGridParams& params, const Handle<Scene>& scene)
    : m_params(params),
      m_scene(scene),
      m_root_node(Handle<Node>(CreateObject<Node>())),
      m_is_initialized(false)
{
    if (scene.IsValid())
    {
        scene->GetRoot()->AddChild(m_root_node);
    }
}

WorldGrid::~WorldGrid()
{
    if (Handle<Scene> scene = m_scene.Lock())
    {
        const RC<EntityManager>& entity_manager = scene->GetEntityManager();

        if (entity_manager != nullptr)
        {
            // wait for all commands to finish to prevent holding dangling references
            entity_manager->GetCommandQueue().AwaitEmpty();
        }
    }

    if (m_root_node.IsValid())
    {
        m_root_node->Remove();
    }
}

void WorldGrid::Initialize()
{
    HYP_SCOPE;

    AssertThrow(!m_is_initialized);

    for (const auto& it : m_plugins)
    {
        it.second->Initialize();
    }

    m_is_initialized = true;
}

void WorldGrid::Shutdown()
{
    HYP_SCOPE;

    AssertThrow(m_is_initialized);

    for (const auto& it : m_plugins)
    {
        it.second->Shutdown();
    }

    m_plugins.Clear();

    m_is_initialized = false;
}

void WorldGrid::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertThrow(m_is_initialized);

    for (const auto& it : m_plugins)
    {
        it.second->Update(delta);
    }

    Handle<Scene> scene = m_scene.Lock();
    AssertThrow(scene.IsValid());

    const RC<EntityManager>& entity_manager = scene->GetEntityManager();
    AssertThrow(entity_manager != nullptr);

    if (m_state.patch_generation_queue_shared.has_updates.Exchange(false, MemoryOrder::ACQUIRE_RELEASE))
    {
        HYP_NAMED_SCOPE("Processing patch generation updates");

        // take the shared queue items and move it to the owned queue for this thread
        m_state.patch_generation_queue_shared.mutex.Lock();
        m_state.patch_generation_queue_owned = std::move(m_state.patch_generation_queue_shared.queue);
        m_state.patch_generation_queue_shared.mutex.Unlock();

        while (m_state.patch_generation_queue_owned.Any())
        {
            HYP_NAMED_SCOPE_FMT("Processing completed patch generation ({} patches ready)", m_state.patch_generation_queue_owned.Size());

            RC<WorldGridPatch> patch = m_state.patch_generation_queue_owned.Pop();
            AssertThrow(patch != nullptr);

            const WorldGridPatchInfo& patch_info = patch->GetPatchInfo();

            { // remove task
                const auto patch_generation_task_it = m_state.patch_generation_tasks.Find(patch_info.coord);

                if (patch_generation_task_it == m_state.patch_generation_tasks.End())
                {
                    HYP_LOG(WorldGrid, Warning, "Generation task for patch at {} no longer in map, must have been removed. Skipping.", patch_info.coord);

                    continue;
                }

                if (patch_generation_task_it->second.IsCompleted())
                {
                    m_state.patch_generation_tasks.Erase(patch_generation_task_it);
                }
                else
                {
                    HYP_LOG(WorldGrid, Warning, "Generation task for patch at {} is not completed. Skipping.", patch_info.coord);
                }
            }

            HYP_LOG(WorldGrid, Info, "Adding generated patch at {}", patch_info.coord);

            Handle<Entity> patch_entity;

            {
                Mutex::Guard guard(m_state.patches_mutex);

                auto it = m_state.patches.Find(patch_info.coord);

                if (it == m_state.patches.End())
                {
                    HYP_LOG(WorldGrid, Warning, "Patch at {} was not found when updating entity", patch_info.coord);

                    continue;
                }

                it->second.patch = patch;

                patch_entity = it->second.entity;
            }

            // @TODO: what should happen if this is hit before the entity is created?

            if (!patch_entity.IsValid())
            {
                HYP_LOG(WorldGrid, Warning, "Patch entity at {} was not found when updating entity", patch_info.coord);

                continue;
            }

            // Initialize patch entity on game thread
            entity_manager->PushCommand([&state = m_state, coord = patch_info.coord, patch = std::move(patch), scene, patch_entity](EntityManager& mgr, GameCounter::TickUnit delta) mutable
                {
                    Threads::AssertOnThread(g_game_thread);

                    patch->InitializeEntity(scene, patch_entity);
                });
        }
    }

    const Handle<Camera>& camera = scene->GetPrimaryCamera();
    const Vec3f camera_position = camera.IsValid() ? camera->GetTranslation() : Vec3f::Zero();
    const Vec2i camera_patch_coord = WorldSpaceToPatchCoord(*this, camera_position);

    // {

    //     Mutex::Guard guard(m_state.patches_mutex);
    //     auto camera_patch_desc_it = m_state.patches.Find(camera_patch_coord);

    //     if (camera_patch_desc_it == m_state.patches.End()) {
    //         // Enqueue a patch to be created at the current camera position
    //         m_state.PushUpdate({
    //             .coord  = camera_patch_coord,
    //             .state  = WorldGridPatchState::WAITING
    //         });
    //     }
    // }

    // process queued updates
    uint32 queue_size = 0;

    while ((queue_size = m_state.patch_update_queue_size.Get(MemoryOrder::ACQUIRE)) != 0)
    {
        HYP_NAMED_SCOPE_FMT("Processing patch updates (queue size: {})", queue_size);

        WorldGridPatchUpdate update;

        { // grab update from queue
            Mutex::Guard guard(m_state.patch_update_queue_mutex);

            update = m_state.patch_update_queue.Pop();

            m_state.patch_update_queue_size.Decrement(1, MemoryOrder::RELEASE);
        }

        switch (update.state)
        {
        case WorldGridPatchState::WAITING:
        {
            HYP_LOG(WorldGrid, Info, "Add patch at {}", update.coord);

            const WorldGridPatchInfo initial_patch_info {
                .extent = m_params.patch_size,
                .coord = update.coord,
                .scale = m_params.scale,
                .state = WorldGridPatchState::LOADED,
                .neighbors = GetPatchNeighbors(update.coord)
            };

            {
                Mutex::Guard guard(m_state.patches_mutex);

                auto patches_it = m_state.patches.Find(update.coord);

                if (patches_it == m_state.patches.End())
                {
                    m_state.patches.Insert(update.coord, WorldGridPatchDesc { initial_patch_info });
                }
            }

            // add command to create the entity
            entity_manager->PushCommand([&root_node = m_root_node, &state = m_state, &params = m_params, patch_info = initial_patch_info](EntityManager& mgr, GameCounter::TickUnit delta)
                {
                    Threads::AssertOnThread(g_game_thread);

                    Handle<Node> patch_node(CreateObject<Node>());
                    patch_node->SetFlags(NodeFlags::TRANSIENT);
                    // patch_node->SetFlags(NodeFlags::TRANSIENT | NodeFlags::HIDE_IN_SCENE_OUTLINE); // temp, debugging performance of having lots of nodes in the list
                    patch_node->SetName(HYP_FORMAT("Patch_{}_{}", patch_info.coord.x, patch_info.coord.y));

                    Handle<Entity> patch_entity = mgr.AddEntity();

                    // Add WorldGridPatchComponent
                    mgr.AddComponent<WorldGridPatchComponent>(patch_entity, WorldGridPatchComponent { .patch_info = patch_info });

                    // Add TransformComponent
                    mgr.AddComponent<TransformComponent>(patch_entity, TransformComponent { .transform = Transform { Vec3f { params.offset.x + (float(patch_info.coord.x) - 0.5f) * (Vec3f(patch_info.extent).Max() - 1.0f) * patch_info.scale.x, params.offset.y, params.offset.z + (float(patch_info.coord.y) - 0.5f) * (Vec3f(patch_info.extent).Max() - 1.0f) * patch_info.scale.z } } });

                    // Add VisibilityStateComponent
                    mgr.AddComponent<VisibilityStateComponent>(patch_entity, VisibilityStateComponent {});

                    // Add BoundingBoxComponent
                    mgr.AddComponent<BoundingBoxComponent>(patch_entity, BoundingBoxComponent {});

                    HYP_LOG(WorldGrid, Info, "Patch entity at {} added", patch_info.coord);

                    {
                        Mutex::Guard guard(state.patches_mutex);

                        auto it = state.patches.Find(patch_info.coord);
                        AssertThrow(it != state.patches.End());

                        it->second.entity = patch_entity;
                    }

                    patch_node->SetEntity(patch_entity);

                    root_node->AddChild(patch_node);
                });

            // @TODO: Only generate patch if entity is created successfully
            if (RC<WorldGridPlugin> plugin = GetMainPlugin())
            {
                auto it = m_state.patch_generation_tasks.Find(initial_patch_info.coord);

                if (it != m_state.patch_generation_tasks.End())
                {
                    HYP_LOG(WorldGrid, Warning, "Patch generation at {} already in progress", initial_patch_info.coord);
                }
                else
                {
                    // add task to generation queue
                    Task<void> generation_task = TaskSystem::GetInstance().Enqueue(
                        HYP_STATIC_MESSAGE("GeneratePatch"),
                        [patch_info = initial_patch_info, &shared_queue = m_state.patch_generation_queue_shared, plugin = std::move(plugin)]()
                        {
                            HYP_NAMED_SCOPE_FMT("Generating patch at {}", patch_info.coord);

                            Mutex::Guard guard(shared_queue.mutex);
                            shared_queue.queue.Push(plugin->CreatePatch(patch_info).ToRefCountedPtr());
                            shared_queue.has_updates.Set(true, MemoryOrder::RELEASE);

                            HYP_LOG(WorldGrid, Info, "Patch generation at {} completed on thread {}", patch_info.coord, Threads::CurrentThreadID().GetName());
                        },
                        TaskThreadPoolName::THREAD_POOL_BACKGROUND);

                    m_state.patch_generation_tasks.Insert(initial_patch_info.coord, std::move(generation_task));
                }
            }
            else
            {
                HYP_LOG(WorldGrid, Warning, "No main plugin found to generate patch at {}", initial_patch_info.coord);
            }

            break;
        }
        case WorldGridPatchState::UNLOADING:
        {
            HYP_LOG(WorldGrid, Info, "Unloading patch at {}", update.coord);

            { // remove associated tasks
                const auto patch_generation_task_it = m_state.patch_generation_tasks.Find(update.coord);

                if (patch_generation_task_it != m_state.patch_generation_tasks.End())
                {
                    Task<void>& patch_generation_task = patch_generation_task_it->second;

                    if (!patch_generation_task.Cancel())
                    {
                        HYP_LOG(WorldGrid, Warning, "Failed to cancel patch generation task at {}", update.coord);

                        patch_generation_task.Await();
                    }

                    m_state.patch_generation_tasks.Erase(patch_generation_task_it);
                }
            }

            { // remove the patch
                Handle<Entity> patch_entity;

                {
                    Mutex::Guard guard(m_state.patches_mutex);

                    const auto patches_it = m_state.patches.Find(update.coord);

                    if (patches_it != m_state.patches.End())
                    {
                        HYP_LOG(WorldGrid, Info, "Patch entity at {} found when unloading", update.coord);

                        patch_entity = std::move(patches_it->second.entity);

                        m_state.patches.Erase(patches_it);
                    }
                }

                if (patch_entity.IsValid())
                {
                    // Push command to remove the entity
                    entity_manager->PushCommand([&state = m_state, update, entity = std::move(patch_entity)](EntityManager& mgr, GameCounter::TickUnit delta)
                        {
                            // tmp; debugging
                            WeakHandle<Node> weak_node;

                            // Remove the node from the parent
                            if (mgr.HasEntity(entity))
                            {
                                if (NodeLinkComponent* node_link_component = mgr.TryGetComponent<NodeLinkComponent>(entity))
                                {
                                    if (Handle<Node> node = node_link_component->node.Lock())
                                    {
                                        weak_node = node; // temp; debugging

                                        node->Remove();
                                    }
                                }
                            }

                            // weak_node.GetRefCountData_Internal()->GetRefTrackData([](const auto &ref_track_data)
                            // {
                            //     HYP_LOG(WorldGrid, Debug, "RefTrackData: {} refs", ref_track_data.Size());
                            //     for (auto &it : ref_track_data) {
                            //         HYP_LOG(WorldGrid, Debug, "\n\tAddress : {}\n\tStack Trace: {}\n\tCount: {}", it.first, it.second.stack_trace, it.second.count);
                            //     }
                            // });
                        });
                }
            }

            m_state.PushUpdate({ .coord = update.coord,
                .state = WorldGridPatchState::UNLOADED });

            break;
        }
        default:
        {
            // // Push command to update patch state
            // entity_manager->PushCommand([&state = m_state, update](EntityManager &mgr, GameCounter::TickUnit delta)
            // {
            //     Threads::AssertOnThread(g_game_thread);

            //     const WorldGridPatchDesc *patch_desc = state.GetPatchDesc(update.coord);
            //     AssertThrow(patch_desc != nullptr);

            //     const ID<Entity> patch_entity = patch_desc->entity;

            //     if (!patch_entity.IsValid()) {
            //         HYP_LOG(WorldGrid, Warning, "Patch entity at {} was not found when updating state", update.coord);

            //         return;
            //     }

            //     WorldGridPatchComponent *patch_component = mgr.TryGetComponent<WorldGridPatchComponent>(patch_entity);
            //     AssertThrowMsg(patch_component, "Patch entity did not have a WorldGridPatchComponent when updating state");

            //     // set new state
            //     patch_component->patch_info.state = update.state;
            // });

            break;
        }
        }
    }

    // get new updates or next iteration
    Array<Vec2i> desired_patch_coords;
    GetDesiredPatches(camera_patch_coord, desired_patch_coords);

    if (desired_patch_coords == m_state.previous_desired_patch_coords)
    {
        return;
    }

    Array<Vec2i> patch_coords_to_add = desired_patch_coords;
    Array<Vec2i> patch_coords_to_remove;

    { // get diff of patches in range
        HYP_NAMED_SCOPE_FMT("Getting diff of patches in range ({} patches desired)", desired_patch_coords.Size());

        Mutex::Guard guard(m_state.patches_mutex);

        for (KeyValuePair<Vec2i, WorldGridPatchDesc>& kv : m_state.patches)
        {
            auto it = desired_patch_coords.Find(kv.first);

            if (it == desired_patch_coords.End())
            {
                patch_coords_to_remove.PushBack(kv.first);
            }
            else
            {
                patch_coords_to_add.Erase(kv.first);
            }
        }
    }

    if (patch_coords_to_remove.Any())
    {
        HYP_NAMED_SCOPE_FMT("Unloading {} patches", patch_coords_to_remove.Size());

        Mutex::Guard guard(m_state.patches_mutex);

        for (const Vec2i& coord : patch_coords_to_remove)
        {
            const auto it = m_state.patches.Find(coord);

            if (it == m_state.patches.End())
            {
                HYP_LOG(WorldGrid, Warning, "Patch at {} was not found when unloading", coord);

                continue;
            }

            const WorldGridPatchInfo& patch_info = it->second.patch_info;

            switch (patch_info.state)
            {
            case WorldGridPatchState::UNLOADED: // fallthrough
            case WorldGridPatchState::UNLOADING:
                break;
            default:
                HYP_LOG(WorldGrid, Info, "Patch {} no longer in range, unloading", patch_info.coord);

                // Start unloading
                m_state.PushUpdate({ .coord = patch_info.coord,
                    .state = WorldGridPatchState::UNLOADING });

                break;
            }
        }
    }

    // enqueue a patch to be created for each patch in range
    if (patch_coords_to_add.Any())
    {
        HYP_NAMED_SCOPE_FMT("Adding {} patches", patch_coords_to_add.Size());

        Mutex::Guard guard(m_state.patches_mutex);

        for (const Vec2i& coord : patch_coords_to_add)
        {
            if (!m_state.patches.Contains(coord))
            {
                m_state.PushUpdate({ .coord = coord,
                    .state = WorldGridPatchState::WAITING });
            }
        }
    }

    m_state.previous_desired_patch_coords = std::move(desired_patch_coords);
}

void WorldGrid::AddPlugin(int priority, RC<WorldGridPlugin>&& plugin)
{
    Threads::AssertOnThread(g_game_thread);

    AssertThrow(plugin != nullptr);

    if (m_is_initialized)
    {
        // Initialize plugin if grid is already initialized

        plugin->Initialize();
    }

    m_plugins.Set(priority, std::move(plugin));
}

RC<WorldGridPlugin> WorldGrid::GetPlugin(int priority) const
{
    const auto it = m_plugins.Find(priority);

    if (it == m_plugins.End())
    {
        return nullptr;
    }

    return it->second;
}

RC<WorldGridPlugin> WorldGrid::GetMainPlugin() const
{
    if (m_plugins.Empty())
    {
        return nullptr;
    }

    return m_plugins.Front().second;
}

void WorldGrid::GetDesiredPatches(Vec2i coord, Array<Vec2i>& out_patch_coords) const
{
    for (int x = MathUtil::Floor(-m_params.max_distance); x <= MathUtil::Ceil(m_params.max_distance) + 1; ++x)
    {
        for (int z = MathUtil::Floor(-m_params.max_distance); z <= MathUtil::Ceil(m_params.max_distance) + 1; ++z)
        {
            out_patch_coords.PushBack(coord + Vec2i { x, z });
        }
    }
}

#pragma endregion WorldGrid

} // namespace hyperion
