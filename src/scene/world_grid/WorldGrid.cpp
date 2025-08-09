/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/world_grid/WorldGrid.hpp>
#include <scene/world_grid/WorldGridLayer.hpp>

#include <scene/Scene.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>

#include <streaming/StreamingManager.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/utilities/ForEach.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(WorldGrid, Scene);

#pragma region WorldGridState

void WorldGridState::PushUpdate(StreamingCellUpdate&& update)
{
    Mutex::Guard guard(patchUpdateQueueMutex);

    patchUpdateQueue.Push(std::move(update));

    patchUpdateQueueSize.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
}

#pragma endregion WorldGridState

#pragma region WorldGrid

WorldGrid::WorldGrid()
    : WorldGrid(nullptr)
{
}

WorldGrid::WorldGrid(World* world)
    : m_world(world),
      m_streamingManager(CreateObject<StreamingManager>(WeakHandleFromThis()))
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
    AddDelegateHandler(g_engineDriver->GetDelegates().OnShutdown.Bind([this]
        {
            if (IsReady())
            {
                Shutdown();
            }
        }));

    InitObject(m_streamingManager);
    m_streamingManager->Start();

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

        m_streamingManager->AddWorldGridLayer(layer);
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

    m_streamingManager->Stop();

    SetReady(false);
}

void WorldGrid::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    AssertReady();

    m_streamingManager->Update(delta);
}

void WorldGrid::AddLayer(const Handle<WorldGridLayer>& layer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

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

        m_streamingManager->AddWorldGridLayer(layer);
    }
}

bool WorldGrid::RemoveLayer(WorldGridLayer* layer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!layer)
    {
        return false;
    }

    auto it = m_layers.FindAs(layer);

    if (it != m_layers.End())
    {
        (*it)->OnRemoved(this);

        m_streamingManager->RemoveWorldGridLayer(*it);

        m_layers.Erase(it);

        return true;
    }

    return false;
}

// void WorldGrid::CreatePatches()
// {
//     HYP_SCOPE;

//     Assert(m_streamingManager.IsValid());

//     m_patches.Clear();
//     m_patches.Resize(m_params.gridSize.Volume());

//     HYP_LOG(WorldGrid, Info, "Creating {} patches for world grid with size {}x{}", m_patches.Size(), m_params.gridSize.x, m_params.gridSize.y);

//     for (uint32 x = 0; x < m_params.gridSize.x; ++x)
//     {
//         for (uint32 z = 0; z < m_params.gridSize.y; ++z)
//         {
//             const Vec2i coord { int(x), int(z) };

//             WorldGridPatchDesc& patchDesc = m_patches[x + z * m_params.gridSize.x];
//             patchDesc.cellInfo = StreamingCellInfo {
//                 .extent = m_params.cellSize,
//                 .coord = coord,
//                 .scale = m_params.scale,
//                 .state = StreamingCellState::UNLOADED,
//                 .neighbors = GetPatchNeighbors(coord)
//             };

//             Handle<StreamingCell> patch = CreateObject<StreamingCell>(this, patchDesc.cellInfo);
//             Assert(patch.IsValid());

//             patchDesc.patch = patch;
//         }
//     }
// }

#pragma endregion WorldGrid

} // namespace hyperion
