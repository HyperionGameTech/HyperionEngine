/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamingManager.hpp>
#include <streaming/StreamingCell.hpp>
#include <streaming/StreamingCellCollection.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/TaskThread.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/containers/Queue.hpp>
#include <core/containers/Forest.hpp>

#include <core/memory/MemoryPool.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/ForEach.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <util/octree/Octree.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region StreamingOctree

class StreamingOctree final : public OctreeBase<StreamingOctree, Handle<StreamableBase>>
{
public:
    StreamingOctree()
        : OctreeBase()
    {
    }

    StreamingOctree(const BoundingBox& aabb)
        : OctreeBase(aabb)
    {
    }

    StreamingOctree(const BoundingBox& aabb, OctreeBase<StreamingOctree, Handle<StreamableBase>>* parent, uint8 index)
        : OctreeBase(aabb, parent, index)
    {
    }

    virtual ~StreamingOctree() override = default;

private:
    virtual UniquePtr<StreamingOctree> CreateChildOctant(const BoundingBox& aabb, StreamingOctree* parent, uint8 index) override
    {
        return MakeUnique<StreamingOctree>(aabb, parent, index);
    }
};

#pragma endregion StreamingOctree

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
public:
    friend class StreamingManager;

    StreamingManagerThread()
        : Thread(ThreadID(Name::Unique("StreamingManagerThread")), ThreadPriorityValue::NORMAL),
          m_is_running { false },
          m_stop_requested { false },
          m_thread_pool(MakeUnique<StreamingThreadPool>())
    {
    }

    /*! \brief Atomically load the boolean value indicating that this thread is actively running */
    bool IsRunning() const
    {
        return m_is_running.Get(MemoryOrder::RELAXED);
    }

    void Stop()
    {
        m_thread_pool->Stop();

        m_stop_requested.Set(true, MemoryOrder::RELAXED);
    }

private:
    virtual void operator()(StreamingManager* streaming_manager) override
    {
        m_is_running.Set(true, MemoryOrder::RELAXED);

        if (StartWorkerThreadPool())
        {
            while (!m_stop_requested.Get(MemoryOrder::RELAXED))
            {
                DoWork(streaming_manager);
            }
        }
        else
        {
            HYP_LOG(Streaming, Error, "Failed to start StreamingWorkerThreadPool!");
        }

        m_is_running.Set(false, MemoryOrder::RELAXED);
    }

    bool StartWorkerThreadPool();
    void DoWork(StreamingManager* streaming_manager);

    AtomicVar<bool> m_is_running;
    AtomicVar<bool> m_stop_requested;

    UniquePtr<StreamingThreadPool> m_thread_pool;

    StreamingCellCollection m_cells;

    /// \todo: Add octree for spatial partitioning of streaming units.
    /// \todo: add buckets for streaming units, based on their state (e.g., loaded, unloading, etc.).
    /// \todo: Add state machine for streaming units to manage their lifecycle. (see WorldGrid)
};

bool StreamingManagerThread::StartWorkerThreadPool()
{
    if (!m_thread_pool->IsRunning())
    {
        m_thread_pool->Start();
        return true;
    }

    return false;
}

void StreamingManagerThread::DoWork(StreamingManager* streaming_manager)
{
    /// \todo implement sync between game thread and streaming thread, using semaphore or condition variable.

    Threads::Sleep(100);

    Queue<Scheduler::ScheduledTask> tasks;

    if (uint32 num_enqueued = m_scheduler.NumEnqueued())
    {
        m_scheduler.AcceptAll(tasks);

        while (tasks.Any())
        {
            tasks.Pop().Execute();
        }
    }

    for (const auto& it : m_cells)
    {
        const Handle<StreamingCell>& cell = it.second;

        if (cell.IsValid())
        {
            HYP_LOG(Streaming, Debug, "Processing StreamingCell with coord: {}", cell->GetPatchInfo().coord);
        }
    }

#if 0
    m_octree.PerformUpdates();

    /// \todo Implement the actual streaming work here.

    auto process_streamables = [&](Span<const Handle<StreamableBase>*> streamables)
    {
        // for (const Handle<StreamableBase>* entry : streamables)
        // {
        //     if (entry->IsValid())
        //     {
        //         HYP_LOG(Streaming, Debug, "Processing StreamableBase with UUID: {}", entry->Get()->GetKey().uuid);
        //     }
        // }

        /// \todo: Update the states of the streamables based on their current state and the streaming volumes they are in.
        /// call OnStreamStart() for newly entered streamables, and enqueue OnLoaded() and OnRemoved() to be called on teh game thread for streamables that are loaded or removed, respectively.
    };

    /// \todo: Process in parallel using the thread pool. Allow tasks to run in background and skip processing if the task batches are still running.
    ForEachStreamingVolume(streaming_manager, [&](StreamingVolumeBase* volume) -> IterationResult
        {
            switch (volume->GetShape())
            {
            case StreamingVolumeShape::BOX:
            {
                BoundingBox aabb;

                if (volume->GetBoundingBox(aabb))
                {
                    Array<const Handle<StreamableBase>*> entries;
                    m_octree.CollectEntries(aabb, entries);

                    process_streamables(entries);
                }
                else
                {
                    HYP_LOG(Streaming, Warning, "StreamingVolumeBase with ID: {} has an invalid bounding box!", volume->GetID().Value());
                }

                break;
            }
            case StreamingVolumeShape::SPHERE:
            {
                BoundingSphere sphere;

                if (volume->GetBoundingSphere(sphere))
                {
                    Array<const Handle<StreamableBase>*> entries;
                    m_octree.CollectEntries(sphere, entries);

                    process_streamables(entries);
                }
                else
                {
                    HYP_LOG(Streaming, Warning, "StreamingVolumeBase with ID: {} has an invalid bounding sphere!", volume->GetID().Value());
                }

                break;
            }
            default:
                HYP_FAIL("Unsupported streaming volume shape!");
            }

            return IterationResult::CONTINUE;
        });
#endif
}

#pragma endregion StreamingManagerThread

#pragma region StreamingManager

// const Handle<StreamingManager>& StreamingManager::GetInstance()
// {
//     return g_streaming_manager;
// }

StreamingManager::StreamingManager()
    : m_thread(MakeUnique<StreamingManagerThread>())
{
    m_scheduler.SetOwnerThread(g_game_thread);
}

StreamingManager::~StreamingManager()
{
}

void StreamingManager::AddStreamingVolume(const Handle<StreamingVolumeBase>& volume)
{
    if (!volume.IsValid())
    {
        return;
    }

    Mutex::Guard guard(m_streaming_volumes_mutex);

    auto it = m_streaming_volumes.Find(volume);

    if (it != m_streaming_volumes.End())
    {
        return;
    }

    m_streaming_volumes.PushBack(volume);
}

void StreamingManager::RemoveStreamingVolume(const Handle<StreamingVolumeBase>& volume)
{
    if (!volume.IsValid())
    {
        return;
    }

    Mutex::Guard guard(m_streaming_volumes_mutex);

    auto it = m_streaming_volumes.Find(volume);

    if (it == m_streaming_volumes.End())
    {
        return;
    }

    m_streaming_volumes.Erase(it);
}

void StreamingManager::RegisterCell(const Handle<StreamingCell>& cell)
{
    HYP_SCOPE;

    if (!cell.IsValid())
    {
        HYP_LOG(Streaming, Warning, "Attempted to register an invalid Streamable cell!");
        return;
    }

    m_thread->GetScheduler().Enqueue([this, cell = cell]()
        {
            InitObject(cell);

            if (Result result = m_thread->m_cells.AddCell(cell); result.HasValue())
            {
                HYP_LOG(Streaming, Debug, "Successfully registered StreamingCell with UUID: {}", cell->GetKey().uuid);

                cell->OnStreamStart();

                // call OnLoaded() on the game thread
                /// \TODO This should all be refactored so that it uses the thread pool to cell OnStreamStart() on and then the main streaming thread can detect completed tasks and call OnLoaded() on the game thread.
                m_scheduler.Enqueue([cell]()
                    {
                        HYP_LOG(Streaming, Debug, "Calling OnLoaded() for StreamingCell with coord: {}", cell->GetPatchInfo().coord);
                        cell->OnLoaded();
                    },
                    TaskEnqueueFlags::FIRE_AND_FORGET);
            }
            else
            {
                HYP_LOG(Streaming, Error, "Failed to register StreamingCell with coord: {}. Error: {}", cell->GetPatchInfo().coord, result.GetError().GetMessage());
            }
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

void StreamingManager::UnregisterCell(const Handle<StreamingCell>& cell)
{
    HYP_SCOPE;

    if (!cell.IsValid())
    {
        HYP_LOG(Streaming, Warning, "Attempted to unregister an invalid StreamingCell!");
        return;
    }

    m_thread->GetScheduler().Enqueue([this, cell = cell]()
        {
            if (Result result = m_thread->m_cells.RemoveCell(cell); result.HasValue())
            {
                // Call OnRemoved() on the game thread
                m_scheduler.Enqueue([cell]()
                    {
                        HYP_LOG(Streaming, Debug, "Unregistering StreamingCell with coord: {}", cell->GetPatchInfo().coord);
                        cell->OnRemoved();
                    },
                    TaskEnqueueFlags::FIRE_AND_FORGET);

                HYP_LOG(Streaming, Debug, "Successfully unregistered StreamingCell with coord: {}", cell->GetPatchInfo().coord);
            }
            else
            {
                HYP_LOG(Streaming, Error, "Failed to unregister StreamingCell with coord: {}. Error: {}", cell->GetPatchInfo().coord, result.GetError().GetMessage());
            }
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

void StreamingManager::RegisterStreamable(const Handle<StreamableBase>& streamable)
{
    HYP_SCOPE;

    if (!streamable.IsValid())
    {
        HYP_LOG(Streaming, Warning, "Attempted to register an invalid Streamable!");
        return;
    }

#if 0
    m_thread->GetScheduler().Enqueue([this, streamable = streamable]()
        {
            InitObject(streamable);

            HYP_LOG(Streaming, Debug, "Registering StreamableBase with UUID: {}", streamable->GetKey().uuid);

            auto insert_result = m_thread->m_octree.Insert(streamable, streamable->GetBoundingBox());

            if (insert_result.first.result)
            {
                HYP_LOG(Streaming, Debug, "Successfully registered StreamableBase with UUID: {}", streamable->GetKey().uuid);

                // Call OnStreamStart() on the game thread
                m_scheduler.Enqueue([streamable]()
                    {
                        streamable->OnStreamStart();
                    },
                    TaskEnqueueFlags::FIRE_AND_FORGET);
            }
            else
            {
                HYP_LOG(Streaming, Error, "Failed to register StreamableBase with UUID: {}. Error: {}", streamable->GetKey().uuid, insert_result.first.message);
            }
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
#endif
}

void StreamingManager::UnregisterStreamable(const UUID& uuid)
{
    HYP_SCOPE;

    if (uuid == UUID::Invalid())
    {
        HYP_LOG(Streaming, Warning, "Attempted to unregister a Streamable with an invalid UUID: {}", uuid);
        return;
    }

#if 0
    m_thread->GetScheduler().Enqueue([this, uuid = uuid]()
        {
            // Find the streaming unit in the octree and remove it
            Array<const Handle<StreamableBase>*> entries;
            m_thread->m_octree.CollectEntries(entries);

            for (const Handle<StreamableBase>* entry : entries)
            {
                if (entry->IsValid() && entry->Get()->GetKey().uuid == uuid)
                {
                    auto result = m_thread->m_octree.Remove(*entry);

                    if (result)
                    {
                        m_scheduler.Enqueue([streamable = *entry]()
                            {
                                // Call OnRemoved() on the game thread
                                streamable->OnRemoved();
                            },
                            TaskEnqueueFlags::FIRE_AND_FORGET);
                    }
                    else
                    {
                        HYP_LOG(Streaming, Error, "Failed to remove StreamableBase with UUID: {} from octree: {}", uuid, result.message);
                    }

                    break;
                }
            }
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
#endif
}

void StreamingManager::UnregisterAllStreamables()
{
    HYP_SCOPE;

#if 0
    if (!m_thread || !m_thread->IsRunning())
    {
        return;
    }

    Array<Handle<StreamableBase>> streamables;

    Task<void> task = m_thread->GetScheduler().Enqueue([this, &streamables]()
        {
            // Clear the octree
            m_thread->m_octree.Clear(streamables, /* undivide */ true);
        });

    task.Await();

    for (const Handle<StreamableBase>& streamable : streamables)
    {
        if (!streamable.IsValid())
        {
            continue;
        }

        // Call OnRemoved() on the game thread
        streamable->OnRemoved();
    }
#endif
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

    m_scheduler.Flush([](auto& operation)
        {
            // No-op, just to ensure all tasks are flushed before leaving
        });
}

void StreamingManager::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
        {
            Stop();
        }));
}

void StreamingManager::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Queue<Scheduler::ScheduledTask> tasks;

    if (uint32 num_enqueued = m_scheduler.NumEnqueued())
    {
        m_scheduler.AcceptAll(tasks);

        while (tasks.Any())
        {
            tasks.Pop().Execute();
        }
    }
}

#pragma endregion StreamingManager

void PostStreamingManagerUpdate(StreamingManager* streaming_manager, Proc<void()>&& proc)
{
    if (!streaming_manager)
    {
        return;
    }

    streaming_manager->m_scheduler.Enqueue(std::move(proc), TaskEnqueueFlags::FIRE_AND_FORGET);
}

void ForEachStreamingVolume(StreamingManager* streaming_manager, ProcRef<IterationResult(StreamingVolumeBase*)> proc)
{
    if (!streaming_manager)
    {
        return;
    }

    Mutex::Guard guard(streaming_manager->m_streaming_volumes_mutex);

    for (const auto& volume : streaming_manager->m_streaming_volumes)
    {
        if (!volume.IsValid())
        {
            continue;
        }

        IterationResult iteration_result = proc(volume.Get());

        if (iteration_result == IterationResult::STOP)
        {
            break;
        }
    }
}

} // namespace hyperion