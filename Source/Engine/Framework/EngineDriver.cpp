/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineStats.hpp>
#include <Framework/EngineMemory.hpp>
#include <Framework/CVarManager.hpp>
#include <Framework/CacheClient.hpp>
#include <Framework/Game.hpp>

#include <Framework/Threads/SimThread.hpp>
#include <Framework/Threads/MainThread.hpp>
#include <Framework/Threads/RenderThread.hpp>
#include <Framework/Threads/RenderWorkerThread.hpp>
#include <Framework/Threads/VisThread.hpp>

#include <Rendering/PostFX.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/FinalPass.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/AsyncCompute.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/Device.hpp>
#include <Rendering/Swapchain.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/DebugDrawer.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/World.hpp>
#include <Scene/View.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Subsystem.hpp>
#include <Scene/InstancedMeshProxy.hpp>
#include <Scene/SystemExecutionGroup.hpp>

#include <Scene/Components/VisibilityStateComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>

#include <Core/FileSystem/FsUtil.hpp>

#include <Core/Debug/StackDump.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Reflection/Enum.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/TaskSystem.hpp>

#include <Core/Core.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/BlobStorage.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Input/Event.hpp>

#if HYP_DOTNET
#include <DotNET/DotNETHost.hpp>
#endif // HYP_DOTNET

#include <System/AppContext.hpp>
#include <System/DirectoryInitializer.hpp>
#include <System/MessageBox.hpp>

#include <HyperionEngine.hpp>

#include <EngineDriver.generated.inl>

#define HYP_PROCESS_VIEWS_ASYNC
#define HYP_PROCESS_SUBSYSTEMS_ASYNC

namespace Hyperion {

void HandleExit();
void HandleSignal(int signum);

#pragma region Stats

EngineStatTimer g_statRenderUpdate("RenderThread");

static EngineStatTimer s_statViewCollection("Sim/ViewCollection");

static EngineStatCounter<uint32> s_statViewsCollected("Sim/ViewsCollected");
static EngineStatCounter<uint32> s_statViewsSkipped("Sim/ViewsSkipped");

#pragma endregion Stats

#pragma region CVars

ThreadSignal g_renderInitSignal { 0 };

// We generally don't have more than 3 Systems running concurrently
CVar<uint32> g_cvNumForegroundWorkerThreads("Threads.NumForegroundWorkers", 3);

#pragma endregion CVars

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

#pragma region Thread Pool Factories

static const Map<TaskThreadPoolName, UniquePtr<TaskThreadPool> (*)(void)> s_threadPoolFactories {
    { TaskThreadPoolName::THREAD_POOL_GENERIC, []() -> UniquePtr<TaskThreadPool>
      {
          return MakeUnique<ForegroundWorkerPool>(g_cvNumForegroundWorkerThreads.Get(), ThreadPriorityValue::HIGHEST);
      } },
    { TaskThreadPoolName::THREAD_POOL_BACKGROUND, []() -> UniquePtr<TaskThreadPool>
      {
          return MakeUnique<BackgroundWorkerPool>("BackgroundWorker", MaxBackgroundWorkerThreads);
      } }
};

#pragma endregion Thread Pool Factories

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
      m_isShuttingDown(false)
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

    g_shaderCompiler = new ShaderCompiler;
    if (!g_shaderCompiler->Initialize())
    {
        HYP_LOG(Engine, Error, "Failed to initialize shader compiler!");
    }

    m_viewCollectionBatch = new TaskBatch();
    
    m_isShuttingDown.Store(false);

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
        AssertDebug(!(world->GetWorldFlags() & WorldFlags::Editor), "Cannot set an editor world as the current world!");
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

    if (m_worlds.Contains(world))
    {
        return;
    }

    world->Initialize();
    
    m_worlds.PushBack(world);
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

            auto pool = createFn();

            if (!pool)
            {
                continue;
            }

            TaskSystem::GetInstance().RegisterPool(name, std::move(pool));
        }

        Assert(m_viewCollectionBatch != nullptr);
        m_viewCollectionBatch->pool = &TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_GENERIC);

        TaskSystem::GetInstance().Start();
    }

    HYP_DEFER({ if (!success) TaskSystem::GetInstance().Stop(); });

    // Needs to be after we init the task system, but before we start our main threads.
    LoadEngineContent();

    success &= g_renderThreadInstance->Start();
    if (!success)
    {
        return false;
    }

    if (g_renderWorkerThreadPool != nullptr)
    {
        g_renderWorkerThreadPool->Start();
    }

#if !HYP_APPLE
    if (g_mainThread != g_renderThread)
    {
        g_renderInitSignal.Wait();
    }
#endif

    success &= g_simThreadInstance->Start();
    if (!success)
    {
        return false;
    }

    success &= g_visThreadInstance->Start();
    if (!success)
    {
        return false;
    }

    success &= g_mainThreadInstance->Start();
    if (!success)
    {
        return false;
    }

    return success;
}

void EngineDriver::Shutdown()
{
    AssertOnThread(g_mainThread);

    if (m_isShuttingDown.Store(true) == true)
    {
        HYP_LOG(Engine, Warning, "Already shutting down!");
        return;
    }

    HYP_LOG(Engine, Info, "Stopping all engine processes");

    m_delegates.OnShutdown();

    if (m_viewCollectionBatch)
    {
        AssertDebug(m_viewCollectionBatch->IsCompleted());

        delete m_viewCollectionBatch;
        m_viewCollectionBatch = nullptr;
    }

    m_worlds.Clear();
}

void EngineDriver::LoadEngineContent()
{
    // Create the engine-global asset registry for shared engine data (shaders, debug shapes, etc.)
    Handle<AssetRegistry> engineRegistry = MakeHandle<AssetRegistry>(
        AssetRegistryId::Engine,
        EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Engine")>());

#if !defined(HYP_SHIPPING) && !defined(HYP_ANDROID)
    ProcRef<void()> doSync;

    auto doSyncImpl = [&]()
    {
        Task<Result> syncContentTask;
        engineRegistry->Initialize(&syncContentTask);

        if (syncContentTask.IsValid())
        {
            HYP_LOG(Assets, Info, "Syncing engine content...");
            Result syncResult = syncContentTask.Await();

            if (syncResult.HasError())
            {
                bool clickedRetry = false;
                bool clickedExit = false;

                CacheClient::SyncFailed(syncResult.GetError(), clickedRetry, clickedExit);

                if (clickedExit)
                {
                    // exit the process
                    std::terminate();
                    return;
                }

                if (clickedRetry)
                {
                    HYP_LOG(Assets, Info, "Retrying engine content sync");

                    syncContentTask = {};
                
                    doSync();

                    return;
                }
            }
        }
    };

    doSync = doSyncImpl;

    // check manifest exists already:
    if ((EngineGlobals::GetCacheDirectory() / "Engine.hmf").Exists())
    {
        // Initialize with no sync.
        engineRegistry->Initialize(nullptr);
    }
    else
    {
        SystemMessageBox(MessageBoxType::INFO)
            .Title("Engine Cache Setup")
            .Text(String("Engine cache needs to be built. Make sure the cache server is running at ") + EngineGlobals::GetCacheServerAddress() + ".\n"
                  + "Press OK to start building the cache.")
            .Button("OK", &doSync)
            .Button("Exit", &std::terminate)
            .Show();
    }
#else
    engineRegistry->Initialize(nullptr);
#endif // !HYP_SHIPPING

    SetEngineAssetRegistry(engineRegistry);
}

void EngineDriver::Simulate(float delta, Game* gameInstance)
{
    static const bool s_dedicatedVisThread = CoreApi::GetCommandLineArguments()["DedicatedVisThread"].ToBool();

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

        world->CollectScenes(scenes);
        world->CollectViews(views);
        world->CollectSubsystems(subsystems);

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

        if (!worldsToRender.Contains(world))
        {
            if ((world->GetWorldFlags() & WorldFlags::Editor))
            {
                // editor world gets rendered first
                worldsToRender.PushFront(world);
            }
            else
            {
                worldsToRender.PushBack(world);
            }
        }
    }

    // Update Worlds and Systems - execution order/batching defined by component descriptors on systems.
    TaskSystem::GetInstance().EnqueueBatch(&worldUpdateTaskBatch);
    worldUpdateTaskBatch.AwaitCompletion();

    // Sanity check: every non-sim-thread SystemExecutionGroup's TaskBatch must be fully
    // completed before we unlock any EntityManager below. If one were still running on a
    // TaskThread, unlocking here would let AddComponent/RemoveComponent/AddTag/RemoveTag calls
    // (from game code or elsewhere on this thread) mutate ComponentContainers and EntitySets
    // out from under a System::Process() call still executing concurrently on another thread.
    const auto assertAllSystemBatchesCompleted = [&simulatingWorlds]()
    {
        for (World* world : simulatingWorlds)
        {
            for (SystemExecutionGroup* systemExecutionGroup : world->GetSystemExecutionGroups())
            {
                if (!systemExecutionGroup->AllowUpdate() || systemExecutionGroup->RequiresSimThread())
                {
                    continue;
                }

                Assert(systemExecutionGroup->GetTaskBatch()->IsCompleted(),
                    "SystemExecutionGroup TaskBatch is not completed - unlocking EntityManagers now would race with an in-flight System::Process() call");
            }
        }
    };

    assertAllSystemBatchesCompleted();

    if (gameInstance != nullptr)
    {
        // Unlock entity managers so the Game instance can mutate
        for (Scene* scene : scenes)
        {
            scene->GetEntityManager()->Unlock();
        }

        gameInstance->OnUpdate(delta);
        gameInstance->m_gameState.gameTime += delta;
        
        // Re-lock
        for (Scene* scene : scenes)
        {
            scene->GetEntityManager()->Lock();
        }
    }

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

#ifdef HYP_PROCESS_SUBSYSTEMS_ASYNC
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
#else  // !HYP_PROCESS_SUBSYSTEMS_ASYNC
    for (Subsystem* subsystem : subsystems)
    {
        subsystem->PreUpdate(delta);
        subsystem->Update(delta);
    }
#endif // HYP_PROCESS_SUBSYSTEMS_ASYNC

    assertAllSystemBatchesCompleted();

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

    // Push rendering data
    {
        if constexpr (!UseRingBuffer)
        {
            BeginSimRenderSyncBlock(&m_isShuttingDown);

            if (HYP_UNLIKELY(m_isShuttingDown.LoadVolatile()))
            {
                return;
            }
        }

        CommitActiveWorlds(worldsToRender.ToSpan());
    }

    { // View collection needs to be in sync with render thread as it will read the committed data
        ENGINE_STAT_SCOPE(&s_statViewCollection);

        for (size_t viewIndex = 0; viewIndex < views.Size(); viewIndex++)
        {
            HYP_NAMED_SCOPE("Per-view entity collection");

            View* view = views[viewIndex];

            if (view->collectionState.skipNext)
            {
                ++s_statViewsSkipped;

                continue;
            }

            ++s_statViewsCollected;

#ifdef HYP_PROCESS_VIEWS_ASYNC
            view->BeginAsyncCollection(*m_viewCollectionBatch);
#else  // !HYP_PROCESS_VIEWS_ASYNC
            view->CollectSync();
#endif // HYP_PROCESS_VIEWS_ASYNC
        }

#ifdef HYP_PROCESS_VIEWS_ASYNC
        TaskSystem::GetInstance().EnqueueBatch(m_viewCollectionBatch);
        m_viewCollectionBatch->AwaitCompletion();

        for (size_t index = 0; index < views.Size(); index++)
        {
            if (views[index]->collectionState.skipNext)
            {
                continue;
            }

            views[index]->EndAsyncCollection();
        }

        AssertDebug(m_viewCollectionBatch != nullptr);
        AssertDebug(m_viewCollectionBatch->IsCompleted());

        m_viewCollectionBatch->ResetState();
#endif // HYP_PROCESS_VIEWS_ASYNC
    }

    {
        // write buffered render data
        WorldShaderData* bufferData = GetWorldBufferData();
        *bufferData = {};
        bufferData->frameCounter = GetFrameCounter();

        if (m_currentWorld)
        {
            bufferData->gameTime = m_currentWorld->GetGameState().gameTime;
        }

        m_viewsPerFrame[slot].Resize(views.Size());
        std::copy(views.Begin(), views.End(), m_viewsPerFrame[slot].Begin());

        DebugDrawer::GetInstance().Update();

        if constexpr (!UseRingBuffer)
        {
            EndSimRenderSyncBlock();
        }
    }

    // End push rendering data

    for (Scene* scene : scenes)
    {
        scene->GetEntityManager()->Unlock();
        scene->GetEntityManager()->AddPendingEntitySets();
    }

    // AfterVis subsystems run after EndSimRenderSyncBlock(), so any debug draws committed (e.g EditorSubsystem, displaying probes)
    // will be seen by the render thread in the next frame.
    for (Subsystem* subsystem : subsystems)
    {
        if (subsystem->GetUpdatePhase() == SubsystemUpdatePhase::AfterVis)
        {
            subsystem->PreUpdate(delta);
            subsystem->Update(delta);
        }
    }
}

#pragma endregion EngineDriver

} // namespace Hyperion
