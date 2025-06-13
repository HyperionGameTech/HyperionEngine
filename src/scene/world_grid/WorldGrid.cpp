/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/world_grid/WorldGrid.hpp>
#include <scene/world_grid/WorldGridPlugin.hpp>

#include <scene/Scene.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>

#include <streaming/StreamingManager.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/utilities/ForEach.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/math/MathUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(WorldGrid, Scene);

#pragma region Helpers

static const FixedArray<StreamingCellNeighbor, 8> GetPatchNeighbors(const Vec2i& coord)
{
    return {
        StreamingCellNeighbor { coord + Vec2i { 1, 0 } },
        StreamingCellNeighbor { coord + Vec2i { -1, 0 } },
        StreamingCellNeighbor { coord + Vec2i { 0, 1 } },
        StreamingCellNeighbor { coord + Vec2i { 0, -1 } },
        StreamingCellNeighbor { coord + Vec2i { 1, -1 } },
        StreamingCellNeighbor { coord + Vec2i { -1, -1 } },
        StreamingCellNeighbor { coord + Vec2i { 1, 1 } },
        StreamingCellNeighbor { coord + Vec2i { -1, 1 } }
    };
}

static Vec2i WorldSpaceToCellCoord(const WorldGrid& world_grid, const Vec3f& world_position)
{
    Vec3f scaled = world_position - world_grid.GetParams().offset;
    scaled *= Vec3f::One() / (world_grid.GetParams().scale * (Vec3f(world_grid.GetParams().cell_size) - 1.0f));
    scaled = MathUtil::Floor(scaled);

    return Vec2i { int(scaled.x), int(scaled.z) };
}

static Vec3f CellCoordToWorldSpace(const WorldGrid& world_grid, const Vec2i& coord)
{
    Vec3f scaled = Vec3f(float(coord.x), 0.0f, float(coord.y));
    scaled *= (world_grid.GetParams().scale * (Vec3f(world_grid.GetParams().cell_size) - 1.0f));
    scaled += world_grid.GetParams().offset;

    return scaled;
}

#pragma endregion Helpers

#pragma region StreamingCell

StreamingCell::StreamingCell(WorldGrid* world_grid, const StreamingCellInfo& cell_info)
    : m_world_grid(world_grid),
      m_cell_info(cell_info)
{
    AssertThrow(m_world_grid != nullptr);
}

StreamingCell::~StreamingCell()
{
}

#pragma endregion StreamingCell

#pragma region WorldGridState

void WorldGridState::PushUpdate(StreamingCellUpdate&& update)
{
    Mutex::Guard guard(patch_update_queue_mutex);

    patch_update_queue.Push(std::move(update));

    patch_update_queue_size.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
}

#pragma endregion WorldGridState

#pragma region WorldGrid

WorldGrid::WorldGrid()
    : WorldGrid(nullptr, WorldGridParams {})
{
}

WorldGrid::WorldGrid(World* world, const WorldGridParams& params)
    : m_world(world),
      m_params(params),
      m_streaming_manager(CreateObject<StreamingManager>())
{
}

WorldGrid::~WorldGrid()
{
}

void WorldGrid::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
        {
            if (IsInitCalled())
            {
                Shutdown();
            }
        }));

    InitObject(m_streaming_manager);
    m_streaming_manager->Start();

    for (const auto& it : m_plugins)
    {
        it.second->Initialize(this);
    }

    // CreatePatches();

    SetReady(true);
}

void WorldGrid::Shutdown()
{
    if (!IsInitCalled())
    {
        return;
    }

    m_streaming_manager->UnregisterAllStreamables();
    m_streaming_manager->Stop();

    // m_patches.Clear();

    for (const auto& it : m_plugins)
    {
        it.second->Shutdown(this);
    }

    m_plugins.Clear();
}

void WorldGrid::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    AssertReady();

    m_streaming_manager->Update(delta);

    for (const auto& it : m_plugins)
    {
        it.second->Update(delta);
    }

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

            Handle<StreamingCell> patch = m_state.patch_generation_queue_owned.Pop();
            AssertThrow(patch.IsValid());
            AssertThrow(InitObject(patch));

            const StreamingCellInfo& cell_info = patch->GetPatchInfo();

            { // remove task
                const auto patch_generation_task_it = m_state.patch_generation_tasks.Find(cell_info.coord);

                if (patch_generation_task_it == m_state.patch_generation_tasks.End())
                {
                    HYP_LOG(WorldGrid, Warning, "Generation task for patch at {} no longer in map, must have been removed. Skipping.", cell_info.coord);

                    continue;
                }

                if (patch_generation_task_it->second.IsCompleted())
                {
                    m_state.patch_generation_tasks.Erase(patch_generation_task_it);
                }
                else
                {
                    HYP_LOG(WorldGrid, Warning, "Generation task for patch at {} is not completed. Skipping.", cell_info.coord);
                }
            }

            HYP_LOG(WorldGrid, Info, "Adding generated patch at {}", cell_info.coord);
        }
    }

    // process queued updates
    uint32 queue_size = 0;

    while ((queue_size = m_state.patch_update_queue_size.Get(MemoryOrder::ACQUIRE)) != 0)
    {
        HYP_NAMED_SCOPE_FMT("Processing patch updates (queue size: {})", queue_size);

        StreamingCellUpdate update;

        { // grab update from queue
            Mutex::Guard guard(m_state.patch_update_queue_mutex);

            update = m_state.patch_update_queue.Pop();

            m_state.patch_update_queue_size.Decrement(1, MemoryOrder::RELEASE);
        }

        switch (update.state)
        {
        case StreamingCellState::WAITING:
        {
            HYP_LOG(WorldGrid, Info, "Add patch at {}", update.coord);

            {
                Mutex::Guard guard(m_state.patches_mutex);

                auto patches_it = m_state.patches.Find(update.coord);

                if (patches_it == m_state.patches.End())
                {
                    const StreamingCellInfo cell_info {
                        .extent = m_params.cell_size,
                        .coord = update.coord,
                        .scale = m_params.scale,
                        .state = StreamingCellState::LOADED,
                        .neighbors = GetPatchNeighbors(update.coord)
                    };

                    Handle<StreamingCell> cell;

                    if (RC<WorldGridPlugin> plugin = GetMainPlugin())
                    {
                        cell = plugin->CreatePatch(this, cell_info);
                    }
                    else
                    {
                        cell = CreateObject<StreamingCell>(this, cell_info);
                    }

                    m_streaming_manager->RegisterCell(cell);

                    m_state.patches.Insert(update.coord, cell);
                }
            }

            break;
        }
        case StreamingCellState::UNLOADING:
        {
            HYP_LOG(WorldGrid, Info, "Unloading patch at {}", update.coord);

            { // remove the patch
                Mutex::Guard guard(m_state.patches_mutex);

                const auto patches_it = m_state.patches.Find(update.coord);

                if (patches_it != m_state.patches.End())
                {
                    auto& patch = patches_it->second;
                    if (patch.IsValid())
                    {
                        m_streaming_manager->UnregisterCell(patch);
                    }

                    m_state.patches.Erase(patches_it);
                }
            }

            m_state.PushUpdate({ .coord = update.coord, .state = StreamingCellState::UNLOADED });

            break;
        }
        default:
        {
            break;
        }
        }
    }

    // get new updates or next iteration
    HashSet<Vec2i> desired_patch_coords;
    GetDesiredPatches(desired_patch_coords);

    const HashCode::ValueType desired_patch_coords_hash = desired_patch_coords.GetHashCode().Value();

    if (m_state.previous_desired_patch_coords_hash == desired_patch_coords_hash)
    {
        // no changes in desired patches, skip
        return;
    }

    Array<Vec2i> patch_coords_to_add = desired_patch_coords.ToArray();
    Array<Vec2i> patch_coords_to_remove;

    { // get diff of patches in range
        HYP_NAMED_SCOPE_FMT("Getting diff of patches in range ({} patches desired)", desired_patch_coords.Size());

        Mutex::Guard guard(m_state.patches_mutex);

        for (KeyValuePair<Vec2i, Handle<StreamingCell>>& kv : m_state.patches)
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

            const Handle<StreamingCell>& cell = it->second;
            AssertThrow(cell.IsValid());

            switch (cell->GetPatchInfo().state)
            {
            case StreamingCellState::UNLOADED: // fallthrough
            case StreamingCellState::UNLOADING:
                break;
            default:
                HYP_LOG(WorldGrid, Info, "Patch {} no longer in range, unloading", cell->GetPatchInfo().coord);

                // Start unloading
                m_state.PushUpdate({ .coord = cell->GetPatchInfo().coord, .state = StreamingCellState::UNLOADING });

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
                m_state.PushUpdate({ .coord = coord, .state = StreamingCellState::WAITING });
            }
        }
    }

    m_state.previous_desired_patch_coords_hash = desired_patch_coords_hash;
}

void WorldGrid::AddPlugin(int priority, const RC<WorldGridPlugin>& plugin)
{
    Threads::AssertOnThread(g_game_thread);

    AssertThrow(plugin != nullptr);

    if (IsInitCalled())
    {
        // Initialize plugin if grid is already initialized

        plugin->Initialize(this);
    }

    m_plugins.Insert(KeyValuePair<int, RC<WorldGridPlugin>>(priority, plugin));
}

RC<WorldGridPlugin> WorldGrid::GetPlugin(int priority) const
{
    const auto it = m_plugins.FindIf([priority](const KeyValuePair<int, RC<WorldGridPlugin>>& kv)
        {
            return kv.first == priority;
        });

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

// void WorldGrid::CreatePatches()
// {
//     HYP_SCOPE;

//     AssertThrow(m_streaming_manager.IsValid());

//     m_patches.Clear();
//     m_patches.Resize(m_params.grid_size.Volume());

//     HYP_LOG(WorldGrid, Info, "Creating {} patches for world grid with size {}x{}", m_patches.Size(), m_params.grid_size.x, m_params.grid_size.y);

//     for (uint32 x = 0; x < m_params.grid_size.x; ++x)
//     {
//         for (uint32 z = 0; z < m_params.grid_size.y; ++z)
//         {
//             const Vec2i coord { int(x), int(z) };

//             WorldGridPatchDesc& patch_desc = m_patches[x + z * m_params.grid_size.x];
//             patch_desc.cell_info = StreamingCellInfo {
//                 .extent = m_params.cell_size,
//                 .coord = coord,
//                 .scale = m_params.scale,
//                 .state = StreamingCellState::UNLOADED,
//                 .neighbors = GetPatchNeighbors(coord)
//             };

//             Handle<StreamingCell> patch = CreateObject<StreamingCell>(this, patch_desc.cell_info);
//             AssertThrow(patch.IsValid());

//             patch_desc.patch = patch;
//         }
//     }
// }

void WorldGrid::GetDesiredPatches(HashSet<Vec2i>& out_patch_coords) const
{
    ForEachStreamingVolume(m_streaming_manager.Get(), [this, &out_patch_coords](StreamingVolumeBase* volume) -> IterationResult
        {
            BoundingBox aabb;

            if (!volume->GetBoundingBox(aabb))
            {
                return IterationResult::CONTINUE;
            }

            const Vec2i coord = WorldSpaceToCellCoord(*this, aabb.GetCenter());

            for (int x = MathUtil::Floor(-m_params.max_distance); x <= MathUtil::Ceil(m_params.max_distance) + 1; ++x)
            {
                for (int z = MathUtil::Floor(-m_params.max_distance); z <= MathUtil::Ceil(m_params.max_distance) + 1; ++z)
                {
                    out_patch_coords.Insert(coord + Vec2i { x, z });
                }
            }

            return IterationResult::CONTINUE;
        });
}

#pragma endregion WorldGrid

} // namespace hyperion
