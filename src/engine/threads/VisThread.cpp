/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/threads/VisThread.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineMemory.hpp>
#include <engine/EngineStats.hpp>

#include <scene/Entity.hpp>
#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/SceneOctree.hpp>
#include <scene/EntityManager.hpp>

#include <scene/components/VisibilityStateComponent.hpp>

#include <core/memory/allocator/SlabAllocator.hpp>

#include <core/threading/Threads.hpp>

namespace Hyperion {

namespace RenderApi {
extern uint32 GetFrameCounter();
} // namespace RenderApi

EngineStatTimer g_visUpdateTimer("Vis/Update");

struct VisUpdateEntry
{
    Entity* entity;
    VisibilityStateComponent* visibilityStateComponent;
    // frame counter value from sim thread when this entry was created
    uint32 frameCounter;
};

#pragma region VisPendingQueue

/// Base SPSC queue from Simulation thread <-> Visibility thread.
/// This template class is specialized into two queue types: Worker queue, and the completed queue.
/// The sim thread pushes to the worker queue and the vis thread pulls them, does work with the enqueued contents, and hands it over to the completed queue.
/// Then the sim thread pops from the completed queue before entity collection.
template <class Derived, class Entry>
class VisWorkerQueue
{
public:
    explicit VisWorkerQueue(uint32 maxSize)
        : m_maxSize(maxSize),
          m_entries(),
          m_head(0),
          m_tail(0)
    {
        m_entries.Resize(maxSize);
    }

    bool HasAny() const
    {
        const uint32 head = std::bit_cast<uint32>(AtomicAdd(&m_head, 0));
        const uint32 tail = std::bit_cast<uint32>(AtomicAdd(&m_tail, 0));

        return !Empty(head, tail);
    }

    void Push(Span<Entry> entries, uint32& outNumRemaining)
    {
        if (entries.Size() == 0)
        {
            return;
        }

        uint32 index = 0;

        while (index < entries.Size())
        {
            const uint32 head = std::bit_cast<uint32>(AtomicAdd(&m_head, 0));
            const uint32 tail = std::bit_cast<uint32>(AtomicAdd(&m_tail, 0));

            if (Full(head, tail))
            {
                return;
            }

            Entry& entry = m_entries[head];
            entry = std::move(entries[index++]);

            outNumRemaining--;

            AtomicExchange(&m_head, (head + 1) & (m_maxSize - 1));
        }
    }

    template <class TArray>
    void Pull(TArray& outArray)
    {
        while (true)
        {
            const uint32 head = std::bit_cast<uint32>(AtomicAdd(&m_head, 0));
            const uint32 tail = std::bit_cast<uint32>(AtomicAdd(&m_tail, 0));

            if (Empty(head, tail))
            {
                return;
            }

            outArray.PushBack(std::move(m_entries[tail]));

            AtomicExchange(&m_tail, (tail + 1) & (m_maxSize - 1));
        }
    }

    // Pop \p numEntities from the queue
    void Pull(uint32 numEntities)
    {
        for (uint32 i = 0; i < numEntities; i++)
        {
            const uint32 head = std::bit_cast<uint32>(AtomicAdd(&m_head, 0));
            const uint32 tail = std::bit_cast<uint32>(AtomicAdd(&m_tail, 0));

            if (Empty(head, tail))
            {
                return;
            }

            AtomicExchange(&m_tail, (tail + 1) & (m_maxSize - 1));
        }
    }

protected:
    HYP_FORCE_INLINE bool Full(uint32 head, uint32 tail) const
    {
        return ((head + 1) & (m_maxSize - 1)) == tail;
    }

    HYP_FORCE_INLINE static bool Empty(uint32 head, uint32 tail)
    {
        return head == tail;
    }

    uint32 m_maxSize;

    Array<Entry, SceneAllocator> m_entries;

    mutable volatile int32 m_head;
    mutable volatile int32 m_tail;
};

class VisPendingQueue final : public VisWorkerQueue<VisPendingQueue, VisUpdateEntry>
{
public:
    explicit VisPendingQueue(uint32 maxSize)
        : VisWorkerQueue(maxSize)
    {
    }

    static VisUpdateEntry CreateEntry(Entity* entity, uint32 frameCounter)
    {
        VisUpdateEntry entry;
        entry.entity = entity;
        entry.frameCounter = frameCounter;

        return entry;
    }
};

class VisCompletedQueue final : public VisWorkerQueue<VisCompletedQueue, Entity*>
{
public:
    explicit VisCompletedQueue(uint32 maxSize)
        : VisWorkerQueue(maxSize)
    {
    }

    static Entity* CreateEntry(Entity* entity)
    {
        return entity;
    }
};

#pragma endregion VisPendingQueue

static bool ProcessEntity(
    Entity* entity,
    VisibilityStateComponent& visibilityStateComponent)
{
    const bool visibilityStateInvalidated = visibilityStateComponent.flags & VisibilityStateFlags::INVALIDATED;

    visibilityStateComponent.flags &= ~VisibilityStateFlags::INVALIDATED;

    Scene* scene = entity->GetScene();
    AssertDebug(scene != nullptr);

    SceneOctree& octree = scene->GetOctree();

    const BoundingBox worldBounds = entity->GetWorldBounds();

    // if entity is not in the octree, try to insert it
    if (visibilityStateComponent.octantId == OctantId::Invalid())
    {
        visibilityStateComponent.visibilityState = nullptr;

        if (!worldBounds.IsValid())
        {
            return false;
        }

        const SceneOctree::Result insertResult = octree.Insert(entity, worldBounds);

        if (insertResult.HasValue())
        {
            AssertDebug(insertResult.GetValue() != OctantId::Invalid(), "Invalid OctantId returned from Insert()");

            visibilityStateComponent.octantId = insertResult.GetValue();

            if (SceneOctree* octant = octree.GetChildOctant(visibilityStateComponent.octantId))
            {
                visibilityStateComponent.visibilityState = &octant->GetVisibilityState();
            }

            return true;
        }

        return false;
    }

    visibilityStateComponent.visibilityState = nullptr;

    // force entry invalidation if the bounding box is not finite,
    // so directional lights changing cause the entire octree to be updated.
    const bool forceEntryInvalidation = visibilityStateInvalidated;

    const SceneOctree::Result updateResult = octree.Update(entity, worldBounds, forceEntryInvalidation);

    if (updateResult.HasError())
    {
        visibilityStateComponent.octantId = OctantId::Invalid();

        HYP_LOG(Scene, Warning, "Failed to update entity {} in octree: {}", entity->Id(), updateResult.GetError().GetMessage());

        return false;
    }

    visibilityStateComponent.octantId = updateResult.GetValue();

    if (visibilityStateComponent.octantId.IsInvalid())
    {
        AssertDebug(false, "Invalid OctantId returned from Update()");

        return false;
    }

    visibilityStateComponent.visibilityState = nullptr;

    if (SceneOctree* octant = octree.GetChildOctant(visibilityStateComponent.octantId))
    {
        visibilityStateComponent.visibilityState = &octant->GetVisibilityState();
    }

    return true;
}

#pragma region VisThread

constexpr uint32 MaxVisUpdates = 2048;
constexpr uint32 TempAllocatorSize = 16 * 1024; // 16KiB per frame ought to be enough

VisThread::VisThread()
    : TaskThread(g_visThread),
      m_tempAllocator(TempAllocatorSize),
      m_pendingQueue(nullptr),
      m_completedQueue(nullptr),
      m_simSemaphore(1),
      m_visSemaphore(0),
      m_frameCounter(0)
{
}

VisThread::~VisThread()
{
    delete m_pendingQueue;
    m_pendingQueue = nullptr;

    delete m_completedQueue;
    m_completedQueue = nullptr;
}

bool VisThread::Start()
{
    if (m_pendingQueue)
        delete m_pendingQueue;

    if (m_completedQueue)
        delete m_completedQueue;

    m_pendingQueue = new VisPendingQueue(MaxVisUpdates);
    m_completedQueue = new VisCompletedQueue(MaxVisUpdates);

    if (m_id == g_simThread) // ! -DedicatedVisThread
    {
        Assert(m_isRunning.Exchange(true, MemoryOrder::ACQUIRE_RELEASE) == false);

        return true;
    }

    return TaskThread::Start();
}

void VisThread::Stop()
{
    TaskThread::Stop();
}

void VisThread::OnFrameStart(uint32 frameCounter)
{
    AssertOnThread(g_simThread);

    m_frameCounter = frameCounter;

    m_simSemaphore.release();
}

void VisThread::OnFrameEnd(Array<Entity*, SceneAllocator>& outProcessedEntities)
{
    AssertOnThread(g_simThread);

    m_visSemaphore.acquire();

    outProcessedEntities.Concat(m_processedEntities);

    m_views.Clear();
    m_processedEntities.Clear();

    m_tempAllocator.Reset();
}

void VisThread::AddViewToProcess(View* view)
{
    AssertOnThread(g_simThread);

    Assert(view != nullptr);

    if (m_views.Contains(view))
    {
        return; // already processing for this frame
    }

    m_views.PushBack(view);
}

void VisThread::Push(
    EntitySetType& entitySet,
    uint32& outNumRemaining)
{
    AssertOnThread(g_simThread);

    outNumRemaining = uint32(entitySet.Size());

    if (entitySet.Size() == 0)
    {
        return;
    }

    for (auto [entity, visibilityStateComponent, _] : entitySet)
    {
        VisUpdateEntry entry {};
        entry.entity = entity;
        entry.visibilityStateComponent = &visibilityStateComponent;
        entry.frameCounter = m_frameCounter;

        m_pendingQueue->Push(Span<VisUpdateEntry>(&entry, 1), outNumRemaining);
    }
}

HYP_DISABLE_OPTIMIZATION;
void VisThread::Process()
{
    ENGINE_STAT_SCOPE(&g_visUpdateTimer);

    m_simSemaphore.acquire();

    Array<Scene*> scenesVisited;

    for (View* view : m_views)
    {
        for (Scene* scene : view->GetScenes())
        {
            if (scenesVisited.Contains(scene))
            {
                continue;
            }

            scenesVisited.PushBack(scene);

            for (auto [entity, visibilityStateComponent, _] : scene->GetEntityManager()->GetEntitySet<VisibilityStateComponent, TagComponent<EntityTag::UpdateVisibility>>().GetScopedView(DataAccessFlags::ACCESS_RW))
            {
                if (ProcessEntity(entity, visibilityStateComponent))
                    m_processedEntities.PushBack(entity);
            }
        }
    }

    m_visSemaphore.release();
}
HYP_ENABLE_OPTIMIZATION;

void VisThread::operator()()
{
    while (!m_stopRequested.Get(MemoryOrder::RELAXED))
    {
        if (m_stopRequested.Get(MemoryOrder::RELAXED))
        {
            break;
        }

        HYP_PROFILE_BEGIN;

        Process();
    }
}

#pragma endregion VisThread

} // namespace Hyperion
