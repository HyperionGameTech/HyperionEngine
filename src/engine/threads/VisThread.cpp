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

extern uint32 GetFrameCounter();

EngineStatTimer g_visUpdateTimer("Vis/Update");

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
      m_simSemaphore(1),
      m_visSemaphore(0),
      m_frameCounter(0)
{
}

VisThread::~VisThread()
{
}

bool VisThread::Start()
{
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

void VisThread::OnFrameEnd(Array<Entity*, SceneTempAllocator>& outProcessedEntities)
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
