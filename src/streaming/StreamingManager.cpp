/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <streaming/StreamingManager.hpp>
#include <streaming/StreamingCell.hpp>
#include <streaming/StreamingCellCollection.hpp>
#include <streaming/StreamingVolume.hpp>

#include <scene/world_grid/WorldGrid.hpp>
#include <scene/world_grid/WorldGridLayer.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/TaskThread.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/reflection/Class.hpp>

#include <core/math/MathUtil.hpp>

#include <core/memory/MemoryPool.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/memory/allocator/ArenaAllocator.hpp>

#include <core/memory/pool/Pool.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <StreamingManager.generated.inl>

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

static Vec2i WorldSpaceToCellCoord(const WorldGridLayerInfo& layerInfo, const Vec3f& worldPosition)
{
    Vec3f scaled = worldPosition - layerInfo.offset;
    scaled *= Vec3f::One() / (layerInfo.scale * (Vec3f(layerInfo.cellSize) - 1.0f));
    scaled = MathUtil::Floor(scaled);

    return { int(scaled.x), int(scaled.z) };
}

#pragma endregion Helpers

#pragma region StreamingWorkerThread

class StreamingWorkerThread : public TaskThread
{
public:
    StreamingWorkerThread(ThreadId id)
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
        : TaskThreadPool(TypeWrapper<StreamingWorkerThread>(), "StreamingWorker", 1)
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
        static constexpr uint32 LockCountMask = 0x7FFFFFFF;
        static constexpr uint32 PendingRemovalBit = 0x80000000;

        Handle<WorldGridLayer> layer;
        StreamingCellCollection<StreamingAllocator> cells;
        Array<StreamingCellUpdate, StreamingAllocator> cellUpdateQueue;
        // highest bit == pending removal flag, so we don't need to add another atomic + padding to eliminate false sharing
        AtomicVar<uint32> lockCount { 0 };

        LayerData(const Handle<WorldGridLayer>& layer)
            : layer(layer)
        {
            Assert(layer.IsValid());
        }

        void Lock()
        {
            lockCount.Increment(1, MemoryOrder::RELEASE);
        }

        void Unlock()
        {
            uint32 value = lockCount.Decrement(1, MemoryOrder::RELEASE);
            AssertDebug(value > 0, "Lock count cannot be negative!");
        }

        bool IsLocked() const
        {
            return (lockCount.Get(MemoryOrder::ACQUIRE) & ~PendingRemovalBit) > 0;
        }

        void SetPendingRemoval()
        {
            lockCount.BitOr(PendingRemovalBit, MemoryOrder::RELEASE);
        }

        bool IsPendingRemoval() const
        {
            return lockCount.Get(MemoryOrder::ACQUIRE) & PendingRemovalBit;
        }
    };

public:
    friend class StreamingManager;

    StreamingManagerThread()
        : Thread(ThreadId(NAME("StreamingManagerThread")), ThreadPriorityValue::NORMAL),
          m_threadPool(MakeUnique<StreamingThreadPool>())
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

        if (!IsRunning() || IsOnThread(Id()))
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
        if (!IsRunning() || IsOnThread(Id()))
        {
            auto it = m_volumes.FindAs(volume);
            Assert(it != m_volumes.End(), "StreamingVolume not found in streaming manager!");

            m_volumes.Erase(it);
        }
        else
        {
            m_scheduler.Enqueue([this, volume]()
                {
                    auto it = m_volumes.FindAs(volume);
                    Assert(it != m_volumes.End(), "StreamingVolume not found in streaming manager!");

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

        if (!IsRunning() || IsOnThread(Id()))
        {
            auto it = m_layers.FindIf([layer](const LayerData& data)
                {
                    return data.layer == layer;
                });

            Assert(it == m_layers.End(), "WorldGridLayer already exists in streaming manager!");

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

                    Assert(it == m_layers.End(), "WorldGridLayer already exists in streaming manager!");

                    m_layers.EmplaceBack(layer);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);

            m_notifier.Produce(1);
        }
    }

    void RemoveWorldGridLayer(const WorldGridLayer* layer)
    {
        if (!IsRunning() || IsOnThread(Id()))
        {
            auto it = m_layers.FindIf([layer](const LayerData& data)
                {
                    return data.layer == layer;
                });

            Assert(it != m_layers.End(), "WorldGridLayer not found in streaming manager!");

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

                    Assert(it != m_layers.End(), "WorldGridLayer not found in streaming manager!");

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
        AssertOnThread(g_gameThread);

        out.Concat(m_cellUpdatesGameThread);
        m_cellUpdatesGameThread.Clear();
    }

    HYP_FORCE_INLINE StreamingNotifier& GetNotifier()
    {
        return m_notifier;
    }

    void Stop() override
    {
        m_threadPool->Stop();

        m_stopRequested.Set(true, MemoryOrder::RELAXED);
        m_notifier.Produce(1); // Wake up the thread if it's waiting on the notifier.
    }

private:
    virtual void operator()(StreamingManager* streamingManager) override
    {
        for (const Handle<StreamingVolumeBase>& volume : m_volumes)
        {
            InitObject(volume);
        }

        for (const LayerData& layerData : m_layers)
        {
            InitObject(layerData.layer);
        }

        StartWorkerThreadPool();

        // Set the notifier to the initial value of 1 so it won't block the first call.
        m_notifier.Produce(1);

        while (!m_stopRequested.Get(MemoryOrder::RELAXED))
        {
            m_notifier.Acquire();

            int32 num = m_notifier.GetValue();

            do
            {
                DoWork(streamingManager);

                // Reset the streaming arena after each work cycle
                // NEED to make sure only one manager thread exists and is working on this arena
                g_streamingArena->Reset();

                num = m_notifier.Release(num);

                AssertDebug(num >= 0); // sanity check
            }
            while (num > 0 && !m_stopRequested.Get(MemoryOrder::RELAXED));

            ThreadSleep(1000);
        }
    }

    void StartWorkerThreadPool();
    void DoWork(StreamingManager* streamingManager);
    void ProcessCellUpdatesForLayer(LayerData& layerData);
    void GetDesiredCellsForLayer(const LayerData& layerData, const Handle<StreamingVolumeBase>& volume, HashSet<Vec2i>& outCellCoords) const;

    void PostCellUpdateToGameThread(Handle<StreamingCell> cell, StreamingCellState state)
    {
        Mutex::Guard guard(m_gameThreadFuturesMutex);

        Task<void>& future = m_gameThreadFutures.EmplaceBack();
        TaskPromise<void>* promise = future.Promise();

        GetThreadById(g_gameThread)->GetScheduler().Enqueue([this, promise, cell = std::move(cell), state]()
            {
                m_cellUpdatesGameThread.EmplaceBack(std::move(cell), state);

                promise->Fulfill();

                Mutex::Guard guard(m_gameThreadFuturesMutex);

                auto it = m_gameThreadFutures.FindIf([promise](const Task<void>& task)
                    {
                        return task.GetTaskExecutor() == promise;
                    });

                Assert(it != m_gameThreadFutures.End(), "Task not found in game thread tasks!");

                m_gameThreadFutures.Erase(it);
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }

    UniquePtr<StreamingThreadPool> m_threadPool;

    Array<Handle<StreamingVolumeBase>, StreamingAllocator> m_volumes;
    LinkedList<LayerData, StreamingAllocator> m_layers;

    Array<Pair<Handle<StreamingCell>, StreamingCellState>> m_cellUpdatesGameThread;
    LinkedList<Task<void>> m_gameThreadFutures;
    Mutex m_gameThreadFuturesMutex;

    StreamingNotifier m_notifier;
};

void StreamingManagerThread::StartWorkerThreadPool()
{
    Assert(m_threadPool != nullptr);
    Assert(!m_threadPool->IsRunning());

    m_threadPool->Start();

    while (!m_threadPool->IsRunning())
    {
        ThreadSleep(0);
    }
}

void StreamingManagerThread::DoWork(StreamingManager* streamingManager)
{
    Array<Scheduler::ScheduledTask, StreamingTempAllocator> tasks;

    if (uint32 numEnqueued = m_scheduler.NumEnqueued())
    {
        m_scheduler.AcceptAll(tasks);

        while (tasks.Any())
        {
            tasks.PopBack().Execute();
        }
    }
    
    for (auto it = m_layers.Begin(); it != m_layers.End();)
    {
        LayerData& layerData = *it;

        if (layerData.IsLocked())
        {
            ++it;

            continue;
        }

        if (layerData.IsPendingRemoval())
        {
            it = m_layers.Erase(it);

            continue;
        }

        WorldGridLayer* layer = layerData.layer;
        AssertDebug(layer != nullptr);
        
        if (!layer)
        {
            ++it;
            
            continue;
        }

        StreamingCellCollection<StreamingAllocator>& cells = layerData.cells;
        Array<StreamingCellUpdate, StreamingAllocator>& cellUpdateQueue = layerData.cellUpdateQueue;

        const WorldGridLayerInfo& layerInfo = layer->GetLayerInfo();

        HashSet<Vec2i> desiredCells;

        for (const Handle<StreamingVolumeBase>& volume : m_volumes)
        {
            if (!volume)
            {
                continue;
            }

            GetDesiredCellsForLayer(layerData, volume, desiredCells);
        }

        // @TODO Use bitset via IDs, or by cell index (x * height + y, would need constant max dimensions for that) to track desired cells and undesired cells.
        Array<Vec2i, StreamingTempAllocator> cellsToAdd = desiredCells.ToArray();
        Array<Handle<StreamingCell>, StreamingTempAllocator> cellsToRemove;

        for (const StreamingCellRuntimeInfo& cellRuntimeInfo : cells)
        {
            auto it = desiredCells.Find(cellRuntimeInfo.coord);

            if (it == desiredCells.End())
            {
                AssertDebug(cellRuntimeInfo.cell != nullptr);

                // Lock so we can use it safely in the loop below for pushing to queue.
                if (!cells.SetCellLockState(cellRuntimeInfo.coord, true))
                {
                    // Already locked, skip adding for removal
                    continue;
                }

                cellsToRemove.PushBack(cellRuntimeInfo.cell);
            }
            else
            {
                // Already have the cell
                cellsToAdd.Erase(cellRuntimeInfo.coord);
            }
        }

        if (cellsToRemove.Any())
        {
            for (const Handle<StreamingCell>& cell : cellsToRemove)
            {
                AssertDebug(cell != nullptr);
                
                if (!cell)
                {
                    continue;
                }
                
                AssertDebug(cells.IsCellLocked(cell->GetPatchInfo().coord),
                    "StreamingCell with coord {} is not locked for unloading!",
                    cell->GetPatchInfo().coord);

                // Cell is locked here -- request unloading.
                cellUpdateQueue.PushBack(StreamingCellUpdate { cell->GetPatchInfo().coord, StreamingCellState::UNLOADING });
            }
        }

        if (cellsToAdd.Any())
        {
            for (const Vec2i& coord : cellsToAdd)
            {
                AssertDebug(!cells.HasCell(coord), "StreamingCell with coord {} already exists!", coord);

                cellUpdateQueue.PushBack(StreamingCellUpdate { coord, StreamingCellState::WAITING });
            }
        }

        ProcessCellUpdatesForLayer(layerData);

        ++it;
    }
}

void StreamingManagerThread::ProcessCellUpdatesForLayer(LayerData& layerData)
{
    const WorldGridLayerInfo& layerInfo = layerData.layer->GetLayerInfo();
    StreamingCellCollection<StreamingAllocator>& cells = layerData.cells;
    Array<StreamingCellUpdate, StreamingAllocator>& cellUpdateQueue = layerData.cellUpdateQueue;

    if (cellUpdateQueue.Empty())
    {
        return;
    }

    Array<Proc<void()>, StreamingAllocator> deferredUpdates;

    while (cellUpdateQueue.Any())
    {
        StreamingCellUpdate update = cellUpdateQueue.PopBack();

        switch (update.state)
        {
        case StreamingCellState::WAITING:
        {
            AssertDebug(!cells.HasCell(update.coord), "StreamingCell with coord {} already exists!", update.coord);

            StreamingCellInfo cellInfo;
            cellInfo.coord = update.coord;
            cellInfo.extent = layerInfo.cellSize;
            cellInfo.scale = layerInfo.scale;
            cellInfo.bounds.min = {
                layerInfo.offset.x + (float(cellInfo.coord.x) - 0.5f) * (float(cellInfo.extent.x) - 1.0f) * cellInfo.scale.x,
                layerInfo.offset.y,
                layerInfo.offset.z + (float(cellInfo.coord.y) - 0.5f) * (float(cellInfo.extent.y) - 1.0f) * cellInfo.scale.z
            };
            cellInfo.bounds.max = cellInfo.bounds.min + Vec3f(cellInfo.extent) * cellInfo.scale;

            Handle<StreamingCell> cell = layerData.layer->CreateStreamingCell(cellInfo);

            if (!cell)
            {
                HYP_LOG(Streaming, Error, "Failed to create StreamingCell for coord: {}", update.coord);

                continue;
            }

            InitObject(cell);

            const bool wasCellAdded = cells.AddCell(cell, StreamingCellState::WAITING, /* lock */ true);
            AssertDebug(wasCellAdded, "Failed to add StreamingCell with coord: {}", update.coord);

            PostCellUpdateToGameThread(cell, StreamingCellState::WAITING);

            layerData.Lock();

            deferredUpdates.EmplaceBack([this, &layerData, cell]()
                {
                    // HYP_LOG(Streaming, Debug, "Loading StreamingCell at coord: {} on thread: {} for layer: {}",
                    //     cell->GetPatchInfo().coord, CurrentThreadId().GetName(), layerData.layer->InstanceClass()->GetName());

                    bool isOk = true;

                    isOk &= layerData.cells.UpdateCellState(cell->GetPatchInfo().coord, StreamingCellState::LOADING);
                    AssertDebug(isOk, "Failed to update StreamingCell state to LOADING for coord: {} for layer: {}",
                        cell->GetPatchInfo().coord,
                        layerData.layer->InstanceClass()->GetName());

                    PostCellUpdateToGameThread(cell, StreamingCellState::LOADING);

                    cell->OnStreamStart();

                    isOk &= layerData.cells.UpdateCellState(cell->GetPatchInfo().coord, StreamingCellState::LOADED);
                    AssertDebug(isOk, "Failed to update StreamingCell state to LOADED for coord: {} for layer: {}\tCurrent state: %{}",
                        cell->GetPatchInfo().coord,
                        layerData.layer->InstanceClass()->GetName(),
                        layerData.cells.GetCellState(cell->GetPatchInfo().coord));

                    isOk &= layerData.cells.SetCellLockState(cell->GetPatchInfo().coord, false);
                    AssertDebug(isOk, "Failed to unlock StreamingCell with coord: {} for layer: {}",
                        cell->GetPatchInfo().coord, layerData.layer->InstanceClass()->GetName());

                    PostCellUpdateToGameThread(cell, StreamingCellState::LOADED);

                    layerData.Unlock();
                });

            break;
        }
        case StreamingCellState::UNLOADING:
        {
            bool isOk = true;

            isOk &= cells.HasCell(update.coord);
            AssertDebug(isOk, "StreamingCell with coord {} does not exist!", update.coord);

            // Locked here - see StreamingManagerThread::DoWork where we lock before pushing UNLOADING state.

            isOk &= cells.IsCellLocked(update.coord);
            AssertDebug(isOk, "StreamingCell with coord {} for layer {} is not locked for unloading!",
                update.coord, layerData.layer->InstanceClass()->GetName());

            Handle<StreamingCell> cell = cells.GetCell(update.coord);
            AssertDebug(cell.IsValid(), "StreamingCell with coord {} for layer {} is not valid!",
                update.coord, layerData.layer->InstanceClass()->GetName());

            isOk &= cells.UpdateCellState(cell->GetPatchInfo().coord, StreamingCellState::UNLOADING);
            AssertDebug(isOk, "Failed to update StreamingCell state to UNLOADING for coord: {} for layer: {}",
                cell->GetPatchInfo().coord,
                layerData.layer->InstanceClass()->GetName());

            PostCellUpdateToGameThread(cell, StreamingCellState::UNLOADING);

            isOk &= cells.RemoveCell(cell->GetPatchInfo().coord);
            AssertDebug(isOk, "Failed to remove StreamingCell with coord: {} for layer: {}",
                cell->GetPatchInfo().coord,
                layerData.layer->InstanceClass()->GetName());

            layerData.Lock();

            // Call OnStreamEnd on the cell and then Unload it
            deferredUpdates.EmplaceBack([this, cell = std::move(cell), &layerData]()
                {
                    // HYP_LOG(Streaming, Debug, "Unloading StreamingCell at coord: {} for layer: {} on thread: {}",
                    //     cell->GetPatchInfo().coord, layerData.layer->InstanceClass()->GetName().LookupString(),
                    //     CurrentThreadId().GetName());

                    PostCellUpdateToGameThread(cell, StreamingCellState::UNLOADED);

                    layerData.Unlock();
                });

            break;
        }
        default:
            break;
        }
    }

    if (deferredUpdates.Any())
    {
        for (auto it = deferredUpdates.Begin(); it != deferredUpdates.End(); ++it)
        {
            TaskSystem::GetInstance().Enqueue(std::move(*it), *m_threadPool, TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }
}

void StreamingManagerThread::GetDesiredCellsForLayer(const LayerData& layerData, const Handle<StreamingVolumeBase>& volume, HashSet<Vec2i>& outCellCoords) const
{
    constexpr Vec2i CellNeighborDirections[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

    const WorldGridLayerInfo& layerInfo = layerData.layer->GetLayerInfo();

    BoundingBox aabb;

    if (!volume->GetBoundingBox(aabb))
    {
        return;
    }

    Array<Vec2f, StreamingTempAllocator> queue;
    HashSet<Vec2i, &KeyBy_Identity<Vec2i>, NodeAllocator<StreamingTempAllocator>> visited;

    const Vec2f centerCoord = Vec2f(WorldSpaceToCellCoord(layerInfo, aabb.GetCenter()));

    queue.PushBack(centerCoord);
    visited.Insert(Vec2i(centerCoord));

    const float maxDistSq = layerInfo.maxDistance * layerInfo.maxDistance;

    while (queue.Any())
    {
        const Vec2f current = queue.PopBack();

        // euclidean distance check
        if (Vec2f(current).DistanceSquared(centerCoord) > maxDistSq)
        {
            continue;
        }

        outCellCoords.Insert(Vec2i(current));

        for (const Vec2i dir : CellNeighborDirections)
        {
            const Vec2f neighbor = current + Vec2f(dir);

            if (visited.Insert(Vec2i(neighbor)).second)
            {
                queue.PushBack(neighbor);
            }
        }
    }
}

#pragma endregion StreamingManagerThread

#pragma region StreamingManager

StreamingManager::StreamingManager()
{
}

StreamingManager::~StreamingManager()
{
    Stop();
}

void StreamingManager::AddStreamingVolume(const Handle<StreamingVolumeBase>& volume)
{
    HYP_SCOPE;

    if (!volume)
    {
        return;
    }

    volume->RegisterNotifier(&m_thread->GetNotifier());

    m_thread->AddStreamingVolume(volume);
}

void StreamingManager::RemoveStreamingVolume(StreamingVolumeBase* volume)
{
    HYP_SCOPE;

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
    
    if (!layer)
    {
        return;
    }

    m_thread->AddWorldGridLayer(layer);
}

void StreamingManager::RemoveWorldGridLayer(WorldGridLayer* layer)
{
    HYP_SCOPE;

    if (!layer)
    {
        return;
    }

    m_thread->RemoveWorldGridLayer(layer);
}

void StreamingManager::Start()
{
    Assert(g_streamingPool != nullptr);

    if (!m_thread)
    {
        m_thread = MakeUnique<StreamingManagerThread>();
    }

    if (m_thread->IsRunning())
    {
        return;
    }

    if (!m_thread->Start(this))
    {
        HYP_FAIL("Failed to start StreamingManagerThread!");
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
    ObjectBase::Init();

    SetReady(true);
}

void StreamingManager::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    Array<Pair<Handle<StreamingCell>, StreamingCellState>> updates;
    m_thread->SinkGameThreadUpdates(updates);

    if (updates.Empty())
    {
        return;
    }

    HYP_LOG(Streaming, Debug, "Update StreamingManager, {} updates", updates.Size());

    for (Pair<Handle<StreamingCell>, StreamingCellState>& update : updates)
    {
        Handle<StreamingCell> cell = std::move(update.first);
        Assert(cell.IsValid(), "StreamingCell is not valid!");

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
