/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/world_grid/WorldGrid.hpp>
#include <scene/world_grid/WorldGridPlugin.hpp>
#include <scene/world_grid/WorldGridLayer.hpp>

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
    : WorldGrid(nullptr)
{
}

WorldGrid::WorldGrid(World* world)
    : m_world(world),
      m_streaming_manager(CreateObject<StreamingManager>(WeakHandleFromThis()))
{
}

WorldGrid::~WorldGrid()
{
    if (IsReady())
    {
        Shutdown();
    }
}

void WorldGrid::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
        {
            if (IsReady())
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

    // Add a default layer if none are provided
    if (m_layers.Empty())
    {
        HYP_LOG(WorldGrid, Info, "No layers provided to WorldGrid, creating default layer");

        m_layers.PushBack(CreateObject<WorldGridLayer>());
    }

    for (const Handle<WorldGridLayer>& layer : m_layers)
    {
        InitObject(layer);

        layer->OnAdded(this);

        m_streaming_manager->AddWorldGridLayer(layer);
    }

    SetReady(true);
}

void WorldGrid::Shutdown()
{
    if (!IsReady())
    {
        return;
    }

    for (const Handle<WorldGridLayer>& layer : m_layers)
    {
        if (!layer.IsValid())
        {
            continue;
        }

        layer->OnRemoved(this);
    }

    m_streaming_manager->Stop();

    for (const auto& it : m_plugins)
    {
        it.second->Shutdown(this);
    }

    m_plugins.Clear();

    SetReady(false);
}

void WorldGrid::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    AssertReady();

    m_streaming_manager->Update(delta);
}

void WorldGrid::AddPlugin(int priority, const RC<WorldGridPlugin>& plugin)
{
    Threads::AssertOnThread(g_game_thread);

    AssertThrow(plugin != nullptr);

    if (IsReady())
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

void WorldGrid::AddLayer(const Handle<WorldGridLayer>& layer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    if (!layer.IsValid())
    {
        return;
    }

    if (m_layers.Contains(layer))
    {
        return;
    }

    m_layers.PushBack(layer);

    if (IsReady())
    {
        InitObject(layer);

        layer->OnAdded(this);

        m_streaming_manager->AddWorldGridLayer(layer);
    }
}

bool WorldGrid::RemoveLayer(WorldGridLayer* layer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    if (!layer)
    {
        return false;
    }

    auto it = m_layers.FindAs(layer);

    if (it != m_layers.End())
    {
        (*it)->OnRemoved(this);

        m_streaming_manager->RemoveWorldGridLayer(*it);

        m_layers.Erase(it);

        return true;
    }

    return false;
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

#pragma endregion WorldGrid

} // namespace hyperion
