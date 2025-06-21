/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamingManager.hpp>
#include <streaming/StreamingCell.hpp>
#include <streaming/StreamingCellCollection.hpp>
#include <streaming/StreamingVolume.hpp>

#include <scene/world_grid/WorldGrid.hpp>
#include <scene/world_grid/WorldGridLayer.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/TaskThread.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/containers/Queue.hpp>

#include <core/object/HypClass.hpp>

#include <core/math/MathUtil.hpp>

#include <core/memory/MemoryPool.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/ForEach.hpp>

#include <core/algorithm/Map.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <util/octree/Octree.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Helpers

static const FixedArray<StreamingCellNeighbor, 8> GetCellNeighbors(const Vec2i& coord)
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

static Vec2i WorldSpaceToCellCoord(const WorldGridLayerInfo& layer_info, const Vec3f& world_position)
{
    Vec3f scaled = world_position - layer_info.offset;
    scaled *= Vec3f::One() / (layer_info.scale * (Vec3f(layer_info.cell_size) - 1.0f));
    scaled = MathUtil::Floor(scaled);

    return { int(scaled.x), int(scaled.z) };
}

#pragma endregion Helpers

#pragma region StreamingWorkerThread

class StreamingWorkerThread : public TaskThread
{
public:
    StreamingWorkerThread(ThreadID id)
        : TaskThread(id, ThreadPriorityValue::LOW)
    {
    }

    virtual ~StreamingWorkerThread() override = default;
};

#pragma endregion StreamingWorkerThread

#pragma region StreamingThreadPool

class StreamingThreadPool : public TaskThreadPool
{
public:
    StreamingThreadPool()
        : TaskThreadPool(TypeWrapper<StreamingWorkerThread>(), "StreamingWorker", 2)
    {
    }

    virtual ~StreamingThreadPool() override = default;
};

#pragma endregion StreamingThreadPool

#pragma region StreamingManagerThread

class StreamingManagerThread final : public Thread<Scheduler, StreamingManager*>
{
    struct LayerData
    {
        enum LayerDataFlags
        {
            LDF_NONE = 0x0,
            LDF_PENDING_REMOVAL = 0x2
        };

        Handle<WorldGridLayer> layer;
        StreamingCellCollection cells;
        Queue<StreamingCellUpdate> cell_update_queue;
        AtomicVar<uint8> flags { LDF_NONE };
        AtomicVar<uint32> lock_count { 0 };

        LayerData(const Handle<WorldGridLayer>& layer)
            : layer(layer)
        {
            AssertThrow(layer.IsValid());
        }

        void Lock()
        {
            lock_count.Increment(1, MemoryOrder::RELEASE);
        }

        void Unlock()
        {
            uint32 value = lock_count.Decrement(1, MemoryOrder::RELEASE);
            AssertDebugMsg(value > 0, "Lock count cannot be negative!");
        }

        bool IsLocked() const
        {
            return lock_count.Get(MemoryOrder::ACQUIRE) > 0;
        }

        void SetPendingRemoval()
        {
            flags.BitOr(LDF_PENDING_REMOVAL, MemoryOrder::RELEASE);
        }

        bool IsPendingRemoval() const
        {
            return flags.Get(MemoryOrder::ACQUIRE) & LDF_PENDING_REMOVAL;
        }
    };

public:
    friend class StreamingManager;

    StreamingManagerThread()
        : Thread(ThreadID(Name::Unique("StreamingManagerThread")), ThreadPriorityValue::NORMAL),
          m_thread_pool(MakeUnique<StreamingThreadPool>())
    {
    }

    virtual ~StreamingManagerThread() override
    {
        for (const Handle<StreamingVolumeBase>& volume : m_volumes)
        {
            if (volume.IsValid())
            {
                volume->UnregisterNotifier(&m_notifier);
            }
        }
    }

    void AddStreamingVolume(const Handle<StreamingVolumeBase>& volume)
    {
        if (!volume.IsValid())
        {
            return;
        }

        if (!IsRunning() || Threads::IsOnThread(GetID()))
        {
            m_volumes.PushBack(volume);
        }
        else
        {
            m_scheduler.Enqueue([this, volume = volume]()
                {
                    m_volumes.PushBack(volume);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);

            m_notifier.Produce(1);
        }
    }

    void RemoveStreamingVolume(const StreamingVolumeBase* volume)
    {
        if (!IsRunning() || Threads::IsOnThread(GetID()))
        {
            auto it = m_volumes.FindAs(volume);
            AssertThrowMsg(it != m_volumes.End(), "StreamingVolume not found in streaming manager!");

            m_volumes.Erase(it);
        }
        else
        {
            m_scheduler.Enqueue([this, volume]()
                {
                    auto it = m_volumes.FindAs(volume);
                    AssertThrowMsg(it != m_volumes.End(), "StreamingVolume not found in streaming manager!");

                    m_volumes.Erase(it);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);

            m_notifier.Produce(1);
        }
    }

    void AddWorldGridLayer(const Handle<WorldGridLayer>& layer)
    {
        if (!layer.IsValid())
        {
            return;
        }

        if (!IsRunning() || Threads::IsOnThread(GetID()))
        {
            auto it = m_layers.FindIf([layer](const LayerData& data)
                {
                    return data.layer == layer;
                });

            AssertThrowMsg(it == m_layers.End(), "WorldGridLayer already exists in streaming manager!");

            m_layers.EmplaceBack(layer);
        }
        else
        {
            m_scheduler.Enqueue([this, layer = layer]()
                {
                    auto it = m_layers.FindIf([layer](const LayerData& data)
                        {
                            return data.layer == layer;
                        });

                    AssertThrowMsg(it == m_layers.End(), "WorldGridLayer already exists in streaming manager!");

                    m_layers.EmplaceBack(layer);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);

            m_notifier.Produce(1);
        }
    }

    void RemoveWorldGridLayer(const WorldGridLayer* layer)
    {
        if (!IsRunning() || Threads::IsOnThread(GetID()))
        {
            auto it = m_layers.FindIf([layer](const LayerData& data)
                {
                    return data.layer == layer;
                });

            AssertThrowMsg(it != m_layers.End(), "WorldGridLayer not found in streaming manager!");

            if (it->IsLocked())
            {
                it->SetPendingRemoval();
                return;
            }

            m_layers.Erase(it);
        }
        else
        {
            m_scheduler.Enqueue([this, layer]()
                {
                    auto it = m_layers.FindIf([layer](const LayerData& data)
                        {
                            return data.layer == layer;
                        });

                    AssertThrowMsg(it != m_layers.End(), "WorldGridLayer not found in streaming manager!");

                    if (it->IsLocked())
                    {
                        it->SetPendingRemoval();
                        return;
                    }

                    m_layers.Erase(it);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);

            m_notifier.Produce(1);
        }
    }

    void SinkGameThreadUpdates(Array<Pair<Handle<StreamingCell>, StreamingCellState>>& out)
    {
        Threads::AssertOnThread(g_game_thread);

        out.Concat(std::move(m_cell_updates_game_thread));
    }

    HYP_FORCE_INLINE StreamingNotifier& GetNotifier()
    {
        return m_notifier;
    }

    void Stop()
    {
        m_thread_pool->Stop();

        m_stop_requested.Set(true, MemoryOrder::RELAXED);
        m_notifier.Produce(1); // Wake up the thread if it's waiting on the notifier.
    }

private:
    virtual void operator()(StreamingManager* streaming_manager) override
    {
        for (const Handle<StreamingVolumeBase>& volume : m_volumes)
        {
            InitObject(volume);
        }

        for (const LayerData& layer_data : m_layers)
        {
            InitObject(layer_data.layer);
        }

        StartWorkerThreadPool();

        // Set the notifier to the initial value of 1 so it won't block the first call.
        m_notifier.Produce(1);

        while (!m_stop_requested.Get(MemoryOrder::RELAXED))
        {
            m_notifier.Acquire();

            int32 num = m_notifier.GetValue();

            do
            {
                DoWork(streaming_manager);

                num = m_notifier.Release(num);

                AssertDebug(num >= 0); // sanity check
            }
            while (num > 0 && !m_stop_requested.Get(MemoryOrder::RELAXED));

            Threads::Sleep(1000);
        }
    }

    void StartWorkerThreadPool();
    void DoWork(StreamingManager* streaming_manager);
    void ProcessCellUpdatesForLayer(LayerData& layer_data);
    void GetDesiredCellsForLayer(const LayerData& layer_data, const Handle<StreamingVolumeBase>& volume, HashSet<Vec2i>& out_cell_coords) const;

    void PostCellUpdateToGameThread(Handle<StreamingCell> cell, StreamingCellState state)
    {
        Mutex::Guard guard(m_game_thread_futures_mutex);

        Task<void>& future = m_game_thread_futures.EmplaceBack();
        TaskPromise<void>* promise = future.Promise();

        Threads::GetThread(g_game_thread)->GetScheduler().Enqueue([this, promise, cell = std::move(cell), state]()
            {
                m_cell_updates_game_thread.EmplaceBack(std::move(cell), state);

                promise->Fulfill();

                Mutex::Guard guard(m_game_thread_futures_mutex);

                auto it = m_game_thread_futures.FindIf([promise](const Task<void>& task)
                    {
                        return task.GetTaskExecutor() == promise;
                    });

                AssertThrowMsg(it != m_game_thread_futures.End(), "Task not found in game thread tasks!");

                m_game_thread_futures.Erase(it);
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }

    UniquePtr<StreamingThreadPool> m_thread_pool;

    Array<Handle<StreamingVolumeBase>> m_volumes;
    LinkedList<LayerData> m_layers;

    Array<Pair<Handle<StreamingCell>, StreamingCellState>> m_cell_updates_game_thread;
    LinkedList<Task<void>> m_game_thread_futures;
    Mutex m_game_thread_futures_mutex;

    StreamingNotifier m_notifier;
};

void StreamingManagerThread::StartWorkerThreadPool()
{
    AssertThrow(m_thread_pool != nullptr);
    AssertThrow(!m_thread_pool->IsRunning());

    m_thread_pool->Start();

    while (!m_thread_pool->IsRunning())
    {
        Threads::Sleep(0);
    }
}

void StreamingManagerThread::DoWork(StreamingManager* streaming_manager)
{
    Queue<Scheduler::ScheduledTask> tasks;

    if (uint32 num_enqueued = m_scheduler.NumEnqueued())
    {
        m_scheduler.AcceptAll(tasks);

        while (tasks.Any())
        {
            tasks.Pop().Execute();
        }
    }

    // HYP_LOG(Streaming, Debug, "Processing streaming work on thread: {}, {} layers, {} volumes, {} cells",
    //     Threads::CurrentThreadID().GetName(),
    //     m_layers.Size(),
    //     m_volumes.Size(),
    //     String::Join(
    //         Map(
    //             m_layers,
    //             [](const LayerData& layer_data)
    //             {
    //                 return HYP_FORMAT("Layer: #{} : {}", layer_data.layer->GetID().Value(), layer_data.cells.Size());
    //             }),
    //         ", "));

    for (auto it = m_layers.Begin(); it != m_layers.End();)
    {
        LayerData& layer_data = *it;

        if (layer_data.IsLocked())
        {
            HYP_LOG(Streaming, Debug, "Layer {} is locked, skipping processing", layer_data.layer->GetLayerInfo().grid_size);

            ++it;

            continue;
        }

        if (layer_data.IsPendingRemoval())
        {
            HYP_LOG(Streaming, Debug, "Layer {} is pending removal, erasing", layer_data.layer->GetLayerInfo().grid_size);

            it = m_layers.Erase(it);

            continue;
        }

        const Handle<WorldGridLayer>& layer = layer_data.layer;
        AssertThrow(layer.IsValid());

        StreamingCellCollection& cells = layer_data.cells;
        Queue<StreamingCellUpdate>& cell_update_queue = layer_data.cell_update_queue;

        HYP_LOG(Streaming, Debug, "Processing layer: {} {}", layer->GetLayerInfo().grid_size, layer->GetLayerInfo().cell_size);

        const WorldGridLayerInfo& layer_info = layer->GetLayerInfo();

        HashSet<Vec2i> desired_cells;

        for (const Handle<StreamingVolumeBase>& volume : m_volumes)
        {
            if (!volume.IsValid())
            {
                continue;
            }

            GetDesiredCellsForLayer(layer_data, volume, desired_cells);
        }

        // @TODO Use bitset via IDs, or by cell index (x * height + y, would need constant max dimensions for that) to track desired cells and undesired cells.
        Array<Vec2i> cells_to_add = desired_cells.ToArray();
        Array<Handle<StreamingCell>> cells_to_remove;

        for (const StreamingCellRuntimeInfo& cell_runtime_info : cells)
        {
            auto it = desired_cells.Find(cell_runtime_info.coord);

            if (it == desired_cells.End())
            {
                AssertThrow(cell_runtime_info.cell.IsValid());

                // Lock so we can use it safely in the loop below for pushing to queue.
                if (!cells.SetCellLockState(cell_runtime_info.coord, true))
                {
                    // Already locked, skip adding for removal
                    continue;
                }

                cells_to_remove.PushBack(cell_runtime_info.cell);
            }
            else
            {
                // Already have the cell
                cells_to_add.Erase(cell_runtime_info.coord);
            }
        }

        if (cells_to_remove.Any())
        {
            for (const Handle<StreamingCell>& cell : cells_to_remove)
            {
                AssertThrow(cell.IsValid());
                AssertDebugMsg(cells.IsCellLocked(cell->GetPatchInfo().coord),
                    "StreamingCell with coord %d,%d is not locked for unloading!",
                    cell->GetPatchInfo().coord.x, cell->GetPatchInfo().coord.y);

                // Cell is locked here -- request unloading.
                cell_update_queue.Push(StreamingCellUpdate { cell->GetPatchInfo().coord, StreamingCellState::UNLOADING });
            }
        }

        if (cells_to_add.Any())
        {
            for (const Vec2i& coord : cells_to_add)
            {
                AssertThrowMsg(!cells.HasCell(coord), "StreamingCell with coord %d,%d already exists!", coord.x, coord.y);

                cell_update_queue.Push(StreamingCellUpdate { coord, StreamingCellState::WAITING });
            }
        }

        ProcessCellUpdatesForLayer(layer_data);

        ++it;
    }
}

void StreamingManagerThread::ProcessCellUpdatesForLayer(LayerData& layer_data)
{
    const WorldGridLayerInfo& layer_info = layer_data.layer->GetLayerInfo();
    StreamingCellCollection& cells = layer_data.cells;
    Queue<StreamingCellUpdate>& cell_update_queue = layer_data.cell_update_queue;

    if (cell_update_queue.Empty())
    {
        return;
    }

    while (cell_update_queue.Any())
    {
        StreamingCellUpdate update = cell_update_queue.Pop();

        HYP_LOG(Streaming, Debug, "Processing StreamingCellUpdate for coord: {}, state: {}", update.coord, update.state);

        switch (update.state)
        {
        case StreamingCellState::WAITING:
        {
            AssertThrowMsg(!cells.HasCell(update.coord), "StreamingCell with coord %d,%d already exists!", update.coord.x, update.coord.y);

            StreamingCellInfo cell_info;
            cell_info.coord = update.coord;
            cell_info.extent = layer_info.cell_size;
            cell_info.scale = layer_info.scale;
            cell_info.bounds.min = {
                layer_info.offset.x + (float(cell_info.coord.x) - 0.5f) * (float(cell_info.extent.x) - 1.0f) * cell_info.scale.x,
                layer_info.offset.y,
                layer_info.offset.z + (float(cell_info.coord.y) - 0.5f) * (float(cell_info.extent.y) - 1.0f) * cell_info.scale.z
            };
            cell_info.bounds.max = cell_info.bounds.min + Vec3f(cell_info.extent) * cell_info.scale;

            Handle<StreamingCell> cell = layer_data.layer->CreateStreamingCell(cell_info);

            if (!cell.IsValid())
            {
                HYP_LOG(Streaming, Error, "Failed to create StreamingCell for coord: {}", update.coord);
                continue;
            }

            InitObject(cell);

            const bool was_cell_added = cells.AddCell(cell, StreamingCellState::WAITING, /* lock */ true);
            AssertThrowMsg(was_cell_added, "Failed to add StreamingCell with coord: %d,%d", update.coord.x, update.coord.y);

            PostCellUpdateToGameThread(cell, StreamingCellState::WAITING);

            layer_data.Lock();

            TaskSystem::GetInstance().Enqueue([this, &layer_data, cell]()
                {
                    HYP_LOG(Streaming, Debug, "Loading StreamingCell at coord: {} on thread: {} for layer: {}",
                        cell->GetPatchInfo().coord, Threads::CurrentThreadID().GetName(), layer_data.layer->InstanceClass()->GetName());

                    bool is_ok = true;

                    is_ok &= layer_data.cells.UpdateCellState(cell->GetPatchInfo().coord, StreamingCellState::LOADING);
                    AssertDebugMsg(is_ok, "Failed to update StreamingCell state to LOADING for coord: %d,%d for layer: %s",
                        cell->GetPatchInfo().coord.x, cell->GetPatchInfo().coord.y,
                        layer_data.layer->InstanceClass()->GetName().LookupString());

                    PostCellUpdateToGameThread(cell, StreamingCellState::LOADING);

                    cell->OnStreamStart();

                    is_ok &= layer_data.cells.UpdateCellState(cell->GetPatchInfo().coord, StreamingCellState::LOADED);
                    AssertDebugMsg(is_ok, "Failed to update StreamingCell state to LOADED for coord: %d,%d for layer: %s\tCurrent state: %u",
                        cell->GetPatchInfo().coord.x, cell->GetPatchInfo().coord.y,
                        layer_data.layer->InstanceClass()->GetName().LookupString(),
                        layer_data.cells.GetCellState(cell->GetPatchInfo().coord));

                    is_ok &= layer_data.cells.SetCellLockState(cell->GetPatchInfo().coord, false);
                    AssertDebugMsg(is_ok, "Failed to unlock StreamingCell with coord: %d,%d for layer: %s",
                        cell->GetPatchInfo().coord.x, cell->GetPatchInfo().coord.y,
                        layer_data.layer->InstanceClass()->GetName().LookupString());

                    PostCellUpdateToGameThread(cell, StreamingCellState::LOADED);

                    layer_data.Unlock();
                },
                *m_thread_pool, TaskEnqueueFlags::FIRE_AND_FORGET);

            break;
        }
        case StreamingCellState::UNLOADING:
        {
            bool is_ok = true;

            is_ok &= cells.HasCell(update.coord);
            AssertThrowMsg(is_ok, "StreamingCell with coord %d,%d does not exist!", update.coord.x, update.coord.y);

            // Locked here - see StreamingManagerThread::DoWork where we lock before pushing UNLOADING state.

            is_ok &= cells.IsCellLocked(update.coord);
            AssertThrowMsg(is_ok, "StreamingCell with coord %d,%d for layer %s is not locked for unloading!",
                update.coord.x, update.coord.y, layer_data.layer->InstanceClass()->GetName().LookupString());

            Handle<StreamingCell> cell = cells.GetCell(update.coord);
            AssertThrowMsg(cell.IsValid(), "StreamingCell with coord %d,%d for layer %s is not valid!",
                update.coord.x, update.coord.y, layer_data.layer->InstanceClass()->GetName().LookupString());

            is_ok &= cells.UpdateCellState(cell->GetPatchInfo().coord, StreamingCellState::UNLOADING);
            AssertDebugMsg(is_ok, "Failed to update StreamingCell state to UNLOADING for coord: %d,%d for layer: %s",
                cell->GetPatchInfo().coord.x, cell->GetPatchInfo().coord.y,
                layer_data.layer->InstanceClass()->GetName().LookupString());

            PostCellUpdateToGameThread(cell, StreamingCellState::UNLOADING);

            is_ok &= cells.RemoveCell(cell->GetPatchInfo().coord);
            AssertDebugMsg(is_ok, "Failed to remove StreamingCell with coord: %d,%d for layer: %s",
                cell->GetPatchInfo().coord.x, cell->GetPatchInfo().coord.y,
                layer_data.layer->InstanceClass()->GetName().LookupString());

            HYP_LOG(Streaming, Debug, "Removed StreamingCell at coord: {} for layer: {} on thread: {}",
                cell->GetPatchInfo().coord, layer_data.layer->InstanceClass()->GetName().LookupString(),
                Threads::CurrentThreadID().GetName());

            layer_data.Lock();

            // Call OnStreamEnd on the cell and then Unload it
            TaskSystem::GetInstance().Enqueue([this, cell = std::move(cell), &layer_data]()
                {
                    HYP_LOG(Streaming, Debug, "Unloading StreamingCell at coord: {} for layer: {} on thread: {}",
                        cell->GetPatchInfo().coord, layer_data.layer->InstanceClass()->GetName().LookupString(),
                        Threads::CurrentThreadID().GetName());

                    // cell->OnStreamEnd();

                    PostCellUpdateToGameThread(cell, StreamingCellState::UNLOADED);

                    layer_data.Unlock();
                },
                *m_thread_pool, TaskEnqueueFlags::FIRE_AND_FORGET);

            break;
        }
        default:
        {
            break;
        }
        }
    }
}

void StreamingManagerThread::GetDesiredCellsForLayer(const LayerData& layer_data, const Handle<StreamingVolumeBase>& volume, HashSet<Vec2i>& out_cell_coords) const
{
    constexpr Vec2i cell_neighbor_directions[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

    const WorldGridLayerInfo& layer_info = layer_data.layer->GetLayerInfo();

    BoundingBox aabb;

    if (!volume->GetBoundingBox(aabb))
    {
        return;
    }

    Queue<Vec2f> queue;
    HashSet<Vec2i> visited;

    const Vec2f center_coord = Vec2f(WorldSpaceToCellCoord(layer_info, aabb.GetCenter()));

    queue.Push(center_coord);
    visited.Insert(Vec2i(center_coord));

    const float max_dist_sq = layer_info.max_distance * layer_info.max_distance;

    while (queue.Any())
    {
        const Vec2f current = queue.Pop();

        // euclidean distance check
        if (Vec2f(current).DistanceSquared(center_coord) > max_dist_sq)
        {
            continue;
        }

        out_cell_coords.Insert(Vec2i(current));

        for (const Vec2i dir : cell_neighbor_directions)
        {
            const Vec2f neighbor = current + Vec2f(dir);

            if (visited.Insert(Vec2i(neighbor)).second)
            {
                queue.Push(neighbor);
            }
        }
    }
}

#pragma endregion StreamingManagerThread

#pragma region StreamingManager

StreamingManager::StreamingManager()
    : StreamingManager(WeakHandle<WorldGrid>())
{
}

StreamingManager::StreamingManager(const WeakHandle<WorldGrid>& world_grid)
    : m_world_grid(world_grid),
      m_thread(MakeUnique<StreamingManagerThread>())
{
}

StreamingManager::~StreamingManager()
{
    Stop();
}

void StreamingManager::AddStreamingVolume(const Handle<StreamingVolumeBase>& volume)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    AssertThrow(volume.IsValid());

    volume->RegisterNotifier(&m_thread->GetNotifier());

    m_thread->AddStreamingVolume(volume);
}

void StreamingManager::RemoveStreamingVolume(StreamingVolumeBase* volume)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    if (!volume)
    {
        return;
    }

    volume->UnregisterNotifier(&m_thread->GetNotifier());

    m_thread->RemoveStreamingVolume(volume);
}

void StreamingManager::AddWorldGridLayer(const Handle<WorldGridLayer>& layer)
{
    HYP_SCOPE;
    // Threads::AssertOnThread(g_game_thread);

    AssertThrow(layer.IsValid());

    m_thread->AddWorldGridLayer(layer);
}

void StreamingManager::RemoveWorldGridLayer(WorldGridLayer* layer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    if (!layer)
    {
        return;
    }

    m_thread->RemoveWorldGridLayer(layer);
}

void StreamingManager::Start()
{
    if (!m_thread->IsRunning())
    {
        m_thread = MakeUnique<StreamingManagerThread>();

        if (!m_thread->Start(this))
        {
            HYP_FAIL("Failed to start StreamingManagerThread!");
        }
    }
}

void StreamingManager::Stop()
{
    if (m_thread->IsRunning())
    {
        m_thread->Stop();
        m_thread.Reset();
    }
}

void StreamingManager::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
        {
            Stop();
        }));

    SetReady(true);
}

void StreamingManager::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    Array<Pair<Handle<StreamingCell>, StreamingCellState>> updates;
    m_thread->SinkGameThreadUpdates(updates);

    if (updates.Empty())
    {
        return;
    }

    for (Pair<Handle<StreamingCell>, StreamingCellState>& update : updates)
    {
        Handle<StreamingCell> cell = std::move(update.first);
        AssertThrowMsg(cell.IsValid(), "StreamingCell is not valid!");

        switch (update.second)
        {
        case StreamingCellState::LOADED:
            cell->OnLoaded();
            break;
        case StreamingCellState::UNLOADED:
            cell->OnRemoved();
            break;
        default:
            break;
        }
    }
}

#pragma endregion StreamingManager

} // namespace hyperion