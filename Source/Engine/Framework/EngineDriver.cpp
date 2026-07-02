/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Framework/EngineStats.hpp>
#include <Framework/EngineMemory.hpp>
#include <Framework/CVarManager.hpp>
#include <Framework/Game.hpp>

#include <Framework/Threads/SimThread.hpp>
#include <Framework/Threads/MainThread.hpp>
#include <Framework/Threads/RenderThread.hpp>
#include <Framework/Threads/VisThread.hpp>

#include <Rendering/PostFX.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/FinalPass.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/RenderCommand.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/AsyncCompute.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/Device.hpp>
#include <Rendering/Swapchain.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/Texture.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/World.hpp>
#include <Scene/View.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Subsystem.hpp>
#include <Scene/InstancedMeshProxy.hpp>

#include <Scene/Components/VisibilityStateComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>

#include <Core/FileSystem/FsUtil.hpp>

#include <Core/Debug/StackDump.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Reflection/Enum.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Core/Net/NetRequestThread.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/TaskSystem.hpp>

#include <Core/Core.hpp>

#include <Asset/Assets.hpp>

#include <Streaming/StreamingManager.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Input/Event.hpp>

#include <System/AppContext.hpp>
#include <System/DirectoryInitializer.hpp>

#include <HyperionEngine.hpp>

#define HYP_PROCESS_VIEWS_ASYNC 1
#define HYP_PROCESS_SUBSYSTEMS_ASYNC 1

#include <EngineDriver.generated.inl>

namespace Hyperion {

void HandleSignal(int signum);

EngineStatTimer g_statRenderUpdate("RenderThread");
EngineStatCounter<uint32> g_statViews("Rendering/Views");

ThreadSignal g_renderInitSignal { 0 };

// We generally don't have more than 3 Systems running concurrently
CVar<uint32> cvNumForegroundWorkerThreads("Threads.NumForegroundWorkers", 3);

#pragma region ForegroundWorkerPool

class ForegroundWorkerPool final : public TaskThreadPool
{
public:
    ForegroundWorkerPool(uint32 numTaskThreads, ThreadPriorityValue priority)
        : TaskThreadPool(TypeWrapper<TaskThread>(), "ForegroundWorker", numTaskThreads)
    {
    }

    virtual ~ForegroundWorkerPool() override = default;
};

#pragma endregion ForegroundWorkerPool

#pragma region RenderWorkerPool

class RenderWorkerPool final : public TaskThreadPool
{
public:
    RenderWorkerPool(uint32 numTaskThreads, ThreadPriorityValue priority)
        : TaskThreadPool(TypeWrapper<TaskThread>(), "RenderWorker", numTaskThreads)
    {
    }

    virtual ~RenderWorkerPool() override = default;
};

#pragma endregion RenderWorkerPool

#pragma region Thread Pool Factories

static const Map<TaskThreadPoolName, UniquePtr<TaskThreadPool> (*)(void)> s_threadPoolFactories {
    { TaskThreadPoolName::THREAD_POOL_GENERIC, []() -> UniquePtr<TaskThreadPool>
      {
          return MakeUnique<ForegroundWorkerPool>(cvNumForegroundWorkerThreads.Get(), ThreadPriorityValue::HIGHEST);
      } },
    { TaskThreadPoolName::THREAD_POOL_RENDER, []() -> UniquePtr<TaskThreadPool>
      {
          return MakeUnique<RenderWorkerPool>(NumRendererWorkerThreads, ThreadPriorityValue::HIGHEST);
      } },
    { TaskThreadPoolName::THREAD_POOL_BACKGROUND, []() -> UniquePtr<TaskThreadPool>
      {
          return MakeUnique<BackgroundWorkerPool>("BackgroundWorker", MaxBackgroundWorkerThreads);
      } }
};

#pragma endregion Thread Pool Factories

void HandleSignal(int signum)
{
#ifdef HYP_WINDOWS
    Win32_CleanupWindowClasses();
#endif

    exit(signum);
}

namespace MeshEntityHelpers {

template <class AllocatorType>
static void UpdateDirtyMeshEntities(Scene* scene, Array<Entity*, AllocatorType>& outUpdatedEntities)
{
    EntityManager* entityManager = scene->GetEntityManager();
    AssertDebug(entityManager != nullptr);

    for (auto [entity, meshComponent, _] : entityManager->GetEntitySet<MeshComponent, TagComponent<EntityTag::UpdateRenderProxy>>())
    {
        entity->SetNeedsRenderProxyUpdate();

        if (meshComponent.previousModelMatrix == entity->GetWorldMatrix())
        {
            outUpdatedEntities.PushBack(entity);
        }
        else
        {
            meshComponent.previousModelMatrix = entity->GetWorldMatrix();
        }
    }
}

} // namespace MeshEntityHelpers

#pragma region EngineDriver

const Handle<EngineDriver>& EngineDriver::GetInstance()
{
    return g_engineDriver;
}

EngineDriver::EngineDriver()
    : m_currentWorld(nullptr),
      m_viewCollectionBatch(nullptr),
      m_isInitialized(false),
      m_isShuttingDown(0)
{
}

EngineDriver::~EngineDriver()
{
}

void EngineDriver::Initialize()
{
    AssertOnThread(g_mainThread);

    if (m_isInitialized)
    {
        return;
    }

    SharedPtr<NetRequestThread> netRequestThread = MakeShared<NetRequestThread>();
    SetGlobalNetRequestThread(netRequestThread);
    netRequestThread->Start();

    // must start after net request thread
    if (CoreApi::IsProfilingEnabled())
    {
        StartProfilerConnectionThread(ProfilerConnectionParams {
            /* endpointUrl */ CoreApi::GetCommandLineArguments()["TraceURL"].ToString(),
            /* enabled */ true });
    }

    m_viewCollectionBatch = new TaskBatch();

    m_isInitialized = true;
}

World* EngineDriver::GetCurrentWorld() const
{
    AssertOnThread(g_simThread);

    return m_currentWorld;
}

void EngineDriver::SetCurrentWorld(World* world)
{
    AssertOnThread(g_simThread);

    if (world == m_currentWorld)
    {
        return;
    }

    if (world)
    {
        AssertDebug(!(world->GetWorldFlags() & WorldFlags::EDITOR_WORLD), "Cannot set an editor world as the current world!");
        AssertDebug(m_worlds.FindAs(world) != m_worlds.End(), "World must be added to the engine before it can be set as the current world!");
    }

    m_currentWorld = world;

    OnCurrentWorldChanged(m_currentWorld);
}

void EngineDriver::AddWorld(const Handle<World>& world)
{
    AssertOnThread(g_simThread);

    if (!world)
    {
        return;
    }

    world->Initialize();

    if (!m_worlds.Contains(world))
    {
        m_worlds.PushBack(world);
    }
}

void EngineDriver::RemoveWorld(const World* world)
{
    AssertOnThread(g_simThread);

    if (!world)
    {
        return;
    }

    auto it = m_worlds.FindIf([world](const Handle<World>& other)
                              {
                                  return other.Get() == world;
                              });

    if (it != m_worlds.End())
    {
        if (m_currentWorld == world)
        {
            SetCurrentWorld(nullptr);
        }

        EnqueueDeletion(std::move(*it));
        m_worlds.Erase(it);
    }
}

Span<View* const> EngineDriver::GetCurrentFrameViews() const
{
    return m_viewsPerFrame[GetRingIndex()].ToSpan();
}

bool EngineDriver::IsRenderLoopActive() const
{
    return g_renderThreadInstance != nullptr
        && g_renderThreadInstance->IsRunning();
}

void EngineDriver::SetGameInstance(Game* gameInstance)
{
    AssertOnThread(g_mainThread);

    Assert(gameInstance != nullptr);

    g_gameInstance = gameInstance;
    g_simThreadInstance->SetGameInstance(gameInstance);
}

Game* EngineDriver::GetGameInstance() const
{
    AssertOnThread(g_mainThread);

    return g_gameInstance;
}

bool EngineDriver::StartThreads()
{
    AssertOnThread(g_mainThread);

    Assert(m_isInitialized);

    Assert(g_renderThreadInstance != nullptr
           && g_simThreadInstance != nullptr
           && g_visThreadInstance != nullptr
           && g_mainThreadInstance != nullptr);

    Assert(!g_renderThreadInstance->IsRunning(), "Render thread is already running!");
    Assert(!g_simThreadInstance->IsRunning(), "Sim thread is already running!");
    Assert(!g_visThreadInstance->IsRunning(), "Vis thread is already running!");

    bool success = true;

    { // Create all task thread pools and initialize the task system
        for (auto& pair : s_threadPoolFactories)
        {
            const TaskThreadPoolName name = pair.first;
            const auto& createFn = pair.second;

            TaskSystem::GetInstance().RegisterPool(name, createFn());
        }

        Assert(m_viewCollectionBatch != nullptr);
        m_viewCollectionBatch->pool = &TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_GENERIC);

        TaskSystem::GetInstance().Start();
    }

    HYP_DEFER({ if (!success) TaskSystem::GetInstance().Stop(); });

    success &= g_renderThreadInstance->Start();
    if (!success)
        return false;

#if !HYP_APPLE
    if (g_mainThread != g_renderThread)
        g_renderInitSignal.Wait();
#endif

    success &= g_simThreadInstance->Start();
    if (!success)
        return false;

    success &= g_visThreadInstance->Start();
    if (!success)
        return false;

    success &= g_mainThreadInstance->Start();
    if (!success)
        return false;

    return success;
}

void EngineDriver::RequestStop()
{
    m_delegates.OnShutdown();

    if (int32 shutdownCounter = AtomicIncrement(&m_isShuttingDown); shutdownCounter == 1)
    {
        if (g_renderThreadInstance != nullptr && g_renderThreadInstance->IsRunning())
        {
            g_renderThreadInstance->Stop();
        }

        if (g_simThreadInstance != nullptr && g_simThreadInstance->IsRunning())
        {
            g_simThreadInstance->Stop();
        }
    }
}

void EngineDriver::FinalizeStop()
{
    AssertOnThread(g_mainThread);

    Assert(AtomicAdd(&m_isShuttingDown, 0) >= 1);

    HYP_LOG(Engine, Info, "Stopping all engine processes");

    if (m_viewCollectionBatch)
    {
        AssertDebug(m_viewCollectionBatch->IsCompleted());

        delete m_viewCollectionBatch;
        m_viewCollectionBatch = nullptr;
    }

    if (TaskSystem::GetInstance().IsRunning())
    {
        TaskSystem::GetInstance().Stop();
    }

    m_worlds.Clear();

    // must stop before net request thread
    StopProfilerConnectionThread();

    if (SharedPtr<NetRequestThread> netRequestThread = GetGlobalNetRequestThread())
    {
        if (netRequestThread->IsRunning())
        {
            netRequestThread->Stop();
        }

        if (netRequestThread->CanJoin())
        {
            netRequestThread->Join();
        }

        SetGlobalNetRequestThread(nullptr);
    }

    m_isShuttingDown = 0;
}

void EngineDriver::UpdateSim(float delta)
{
    static const bool s_dedicatedVisThread = CoreApi::GetCommandLineArguments()["DedicatedVisThread"].ToBool();

    g_streamingManager->Update(delta);

    const uint32 slot = GetRingIndex();
    const uint32 frameCounter = GetFrameCounter();

    Array<Scene*, SceneTempAllocator> scenes;
    Array<View*, SceneTempAllocator> views;
    Array<Subsystem*, SceneTempAllocator> subsystems;

    TaskBatch worldUpdateTaskBatch;
    TaskBatch* currBatch = &worldUpdateTaskBatch;

    Array<World*, SceneTempAllocator> worldsToRender;
    worldsToRender.Reserve(m_worlds.Size());

    Array<World*, SceneTempAllocator> simulatingWorlds;
    simulatingWorlds.Reserve(m_worlds.Size());

    for (uint32 i = 0; i < uint32(m_worlds.Size()); i++)
    {
        World* world = m_worlds[i];

        const GameState& gameState = world->GetGameState();

        // if (!gameState.IsStopped())
        // {
        world->CollectScenes(scenes);
        world->CollectViews(views);
        world->CollectSubsystems(subsystems);

        g_statViews += views.Size();

        // if (gameState.IsSimulating() || (world->GetWorldFlags() & WorldFlags::EDITOR_WORLD))
        //{
        simulatingWorlds.PushBack(world);

        world->BeginUpdate(*currBatch, delta);

        if (i != uint32(m_worlds.Size() - 1))
        {
            // get the tail to pass to the next world's BeginUpdate()
            while (currBatch->nextBatch != nullptr)
            {
                currBatch = currBatch->nextBatch;
            }
        }
        //}

        if (!worldsToRender.Contains(world))
        {
            if ((world->GetWorldFlags() & WorldFlags::EDITOR_WORLD))
            {
                // editor world gets rendered first
                worldsToRender.PushFront(world);
            }
            else
            {
                worldsToRender.PushBack(world);
            }
        }
        // }
    }

    CommitActiveWorlds(worldsToRender.ToSpan());

    // Update Worlds and Systems - execution order/batching defined by component descriptors on systems.
    TaskSystem::GetInstance().EnqueueBatch(&worldUpdateTaskBatch);
    worldUpdateTaskBatch.AwaitCompletion();

    { // collect shadow views
        const size_t initialNumViews = views.Size();

        for (size_t viewIndex = 0; viewIndex < initialNumViews; viewIndex++)
        {
            View* view = views[viewIndex];
            Assert(view != nullptr);

            if (view->ShouldCollectShadowViews())
            {
                view->PrepareShadowViews(views);
            }
        }

        RI.shadowMapCache->Update();
    }

    static const auto removeNonUnique = []<class ArrayType>(ArrayType& elems)
    {
        for (size_t idx = 0; idx < elems.Size();)
        {
            if (elems.Find(elems[idx]) != elems.begin() + idx)
            {
                elems.Erase(elems.begin() + idx);

                continue;
            }

            ++idx;
        }
    };

    removeNonUnique(views);
    removeNonUnique(subsystems);
    removeNonUnique(scenes);

    for (View* view : views)
    {
        view->UpdateViewport();

        g_visThreadInstance->AddViewToProcess(view);
    }

    g_visThreadInstance->OnFrameStart(frameCounter);

    if (!s_dedicatedVisThread)
    {
        g_visThreadInstance->Process();
    }

#if HYP_PROCESS_SUBSYSTEMS_ASYNC
    Array<Task<void>, SceneTempAllocator> updateSubsystemTasks;
    updateSubsystemTasks.Reserve(subsystems.Size());

    for (Subsystem* subsystem : subsystems)
    {
        if (subsystem->GetUpdatePhase() != SubsystemUpdatePhase::BeforeVis || subsystem->RequiresUpdateOnSimThread())
        {
            continue;
        }

        subsystem->PreUpdate(delta);

        updateSubsystemTasks.PushBack(TaskSystem::GetInstance().Enqueue(
            [subsystem, delta]
            {
                HYP_NAMED_SCOPE_FMT("Update subsystem: {}", subsystem->InstanceClass()->GetName());

                subsystem->Update(delta);
            }));
    }

    for (Subsystem* subsystem : subsystems)
    {
        if (subsystem->GetUpdatePhase() != SubsystemUpdatePhase::BeforeVis || !subsystem->RequiresUpdateOnSimThread())
        {
            continue;
        }

        subsystem->PreUpdate(delta);
        subsystem->Update(delta);
    }

    for (Task<void>& task : updateSubsystemTasks)
    {
        task.Await();
    }

    updateSubsystemTasks.Clear();
#else
    for (Subsystem* subsystem : m_subsystemsArray)
    {
        subsystem->PreUpdate(delta);
        subsystem->Update(delta);
    }
#endif

    for (Scene* scene : scenes)
    {
        scene->GetEntityManager()->Unlock();
    }

    for (World* world : simulatingWorlds)
    {
        world->EndUpdate();
    }

    enum UpdatedEntitiesBucket
    {
        Bucket_RenderProxy,
        Bucket_Visibility,
        Bucket_Max
    };

    Array<Entity*, SceneTempAllocator> updatedEntities[Bucket_Max];

    {
        // update mark render proxies as needing update for all entities that could be visible,
        // if they have the UpdateRenderProxy tag
        Array<Scene*, SceneTempAllocator> visitedScenes;
        visitedScenes.Reserve(8);

        for (View* view : views)
        {
            for (Scene* scene : view->GetScenes())
            {
                if (visitedScenes.Contains(scene))
                {
                    continue;
                }

                MeshEntityHelpers::UpdateDirtyMeshEntities(scene, updatedEntities[Bucket_RenderProxy]);

                visitedScenes.PushBack(scene);
            }
        }
    }

    g_visThreadInstance->OnFrameEnd(updatedEntities[Bucket_Visibility]);

    for (Scene* scene : scenes)
    {
        // add pending before we mutate entity sets via removing tags.
        // if we don't, then the states of the pending entity sets may become stale
        // (they may still assume the tag components exist)
        scene->GetEntityManager()->AddPendingEntitySets();
    }

    // remove tags for updates that were applied
    if (std::any_of(std::begin(updatedEntities), std::end(updatedEntities), [](const auto& arr)
                    {
                        return arr.Any();
                    }))
    {
        for (Entity* entity : updatedEntities[Bucket_RenderProxy])
        {
            entity->RemoveTag<EntityTag::UpdateRenderProxy>();
        }

        for (Entity* entity : updatedEntities[Bucket_Visibility])
        {
            entity->RemoveTag<EntityTag::UpdateVisibility>();
        }
    }

    // relock
    for (Scene* scene : scenes)
    {
        scene->GetEntityManager()->Lock();
    }

    for (size_t viewIndex = 0; viewIndex < views.Size(); viewIndex++)
    {
        HYP_NAMED_SCOPE("Per-view entity collection");

        View* view = views[viewIndex];

        view->UpdateVisibility();

#if HYP_PROCESS_VIEWS_ASYNC
        view->BeginAsyncCollection(*m_viewCollectionBatch);
#else
        view->CollectSync();
#endif
    }

#if HYP_PROCESS_VIEWS_ASYNC
    TaskSystem::GetInstance().EnqueueBatch(m_viewCollectionBatch);
    m_viewCollectionBatch->AwaitCompletion();

    for (size_t index = 0; index < views.Size(); index++)
    {
        views[index]->EndAsyncCollection();
    }
#endif

#if HYP_PROCESS_VIEWS_ASYNC
    AssertDebug(m_viewCollectionBatch != nullptr);
    AssertDebug(m_viewCollectionBatch->IsCompleted());

    m_viewCollectionBatch->ResetState();
#endif

    for (Scene* scene : scenes)
    {
        scene->GetEntityManager()->Unlock();
        scene->GetEntityManager()->AddPendingEntitySets();
    }

    for (Subsystem* subsystem : subsystems)
    {
        if (subsystem->GetUpdatePhase() == SubsystemUpdatePhase::AfterVis)
        {
            subsystem->PreUpdate(delta);
            subsystem->Update(delta);
        }
    }

    // write buffered render data
    WorldShaderData* bufferData = GetWorldBufferData();
    bufferData->frameCounter = GetFrameCounter();

    if (m_currentWorld)
    {
        bufferData->gameTime = m_currentWorld->GetGameState().gameTime;
    }

    m_viewsPerFrame[slot].Resize(views.Size());

    std::copy(views.Begin(), views.End(), m_viewsPerFrame[slot].Begin());
}

#pragma endregion EngineDriver

} // namespace Hyperion
