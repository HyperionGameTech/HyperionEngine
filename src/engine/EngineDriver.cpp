/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineGlobals.hpp>
#include <engine/EngineStats.hpp>
#include <engine/EngineMemory.hpp>
#include <engine/DebugDrawer.hpp>
#include <engine/Game.hpp>

#include <engine/threads/SimThread.hpp>
#include <engine/threads/MainThread.hpp>
#include <engine/threads/RenderThread.hpp>
#include <engine/threads/VisThread.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/AsyncCompute.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/Device.hpp>
#include <rendering/Swapchain.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Texture.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/EntityManager.hpp>
#include <scene/Subsystem.hpp>

#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/MeshComponent.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/debug/StackDump.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/reflection/Enum.hpp> // For EnumValue()

#include <core/cli/CommandLine.hpp>

#include <core/net/NetRequestThread.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <asset/Assets.hpp>

#include <streaming/StreamingManager.hpp>

#include <util/MeshBuilder.hpp>

#include <input/Event.hpp>

#include <system/AppContext.hpp>
#include <system/App.hpp>

#include <scripting/ScriptingService.hpp>

#include <HyperionEngine.hpp>

#define HYP_PROCESS_VIEWS_ASYNC 1
#define HYP_PROCESS_SUBSYSTEMS_ASYNC 1

#include <EngineDriver.generated.inl>

namespace Hyperion {

void HandleSignal(int signum);

namespace CoreApi {
extern const GlobalConfig& GetGlobalConfig();
extern FilePath GetExecutablePath();
extern const CommandLineArguments& GetCommandLineArguments();
} // namespace CoreApi

EngineStatTimer g_renderTimer("Frame/Render");

std::binary_semaphore g_renderThreadInit { 0 };

void HandleSignal(int signum)
{
#ifdef HYP_WINDOWS
    Win32_CleanupWindowClasses();
#endif

    exit(signum);
}

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

#pragma region EngineDriver

const Handle<EngineDriver>& EngineDriver::GetInstance()
{
    return g_engineDriver;
}

EngineDriver::EngineDriver()
    : m_currentWorld(nullptr),
      m_viewCollectionBatch(nullptr),
      m_isShuttingDown(0)
{
}

EngineDriver::~EngineDriver()
{
}

HYP_API void EngineDriver::Init()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

#ifdef HYP_EDITOR
    // Create script compilation service
    m_scriptingService = MakeUnique<ScriptingService>(
        GetResourceDirectory() / "scripts" / "src",
        GetResourceDirectory() / "scripts" / "projects",
        CoreApi::GetExecutablePath()); // copy script binaries into executable path

    m_scriptingService->Start();
#endif

    RC<NetRequestThread> netRequestThread = MakeRefCountedPtr<NetRequestThread>();
    SetGlobalNetRequestThread(netRequestThread);
    netRequestThread->Start();

    // must start after net request thread
    if (CoreApi::GetCommandLineArguments()["Profile"])
    {
        StartProfilerConnectionThread(ProfilerConnectionParams {
            /* endpointUrl */ CoreApi::GetCommandLineArguments()["TraceURL"].ToString(),
            /* enabled */ true });
    }

    m_debugDrawer = MakeHandle<DebugDrawer>();

    m_viewCollectionBatch = new TaskBatch();
    m_viewCollectionBatch->pool = &TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_GENERIC);

    SetReady(true);
}

World* EngineDriver::GetCurrentWorld() const
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    return m_currentWorld;
}

void EngineDriver::SetCurrentWorld(World* world)
{
    HYP_SCOPE;
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

void EngineDriver::SetDefaultWorld(const Handle<World>& defaultWorld)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    m_defaultWorld = defaultWorld;

    if (IsInitCalled())
    {
        InitObject(m_defaultWorld);
    }
}

void EngineDriver::AddWorld(const Handle<World>& world)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (!world)
    {
        return;
    }

    InitObject(world);

    if (!m_worlds.Contains(world))
    {
        m_worlds.PushBack(world);
    }
}

void EngineDriver::RemoveWorld(const World* world)
{
    HYP_SCOPE;
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

        SafeDelete(std::move(*it));
        m_worlds.Erase(it);
    }
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

    Handle<Game> gameInstanceStrong = MakeStrongRef(gameInstance);

    g_gameInstance = gameInstanceStrong;
    g_simThreadInstance->SetGame(gameInstanceStrong);
}

Game* EngineDriver::GetGameInstance() const
{
    AssertOnThread(g_mainThread);

    return g_gameInstance;
}

bool EngineDriver::StartThreads()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);
    AssertReady();

    Assert(g_renderThreadInstance != nullptr
        && g_simThreadInstance != nullptr
        && g_visThreadInstance != nullptr
        && g_mainThreadInstance != nullptr);

    Assert(!g_renderThreadInstance->IsRunning(), "Render thread is already running!");
    Assert(!g_simThreadInstance->IsRunning(), "Sim thread is already running!");
    Assert(!g_visThreadInstance->IsRunning(), "Vis thread is already running!");

    bool success = true;

    success &= g_renderThreadInstance->Start();
    if (!success)
        return false;

    success &= g_simThreadInstance->Start();
    if (!success)
        return false;

    success &= g_visThreadInstance->Start();
    if (!success)
        return false;

    return g_mainThreadInstance->Start();
}

void EngineDriver::RequestStop()
{
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
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    Assert(AtomicAdd(&m_isShuttingDown, 0) >= 1);

    HYP_LOG(Engine, Info, "Stopping all engine processes");

    m_delegates.OnShutdown();

    if (m_scriptingService)
    {
        m_scriptingService->Stop();
        m_scriptingService.Reset();
    }

    if (m_viewCollectionBatch)
    {
        AssertDebug(m_viewCollectionBatch->IsCompleted());

        delete m_viewCollectionBatch;
        m_viewCollectionBatch = nullptr;
    }

    // must stop before net request thread
    StopProfilerConnectionThread();

    if (RC<NetRequestThread> netRequestThread = GetGlobalNetRequestThread())
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

    SafeDelete(std::move(m_worlds));

    m_debugDrawer.Reset();

    // delete remaining enqueued deletions.
    // loop until all deletions are done

    // clang-format off
    FixedArray<int, RingBufferDepth> counts {};
    
    do
    {
        for (uint32 i = 0; i < RingBufferDepth; i++)
        {
            counts[i] = g_safeDeleter->ForceDeleteAll(i);
        }

        ThreadSleep(1); // give some time for other threads to finish
    }
    while (AnyOf(counts, [](uint32 count) { return count > 0; }));
    // clang-format on

    m_isShuttingDown = 0;
}

void EngineDriver::PreFrameUpdate(Frame* frame)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);
}

void EngineDriver::UpdateSim(float delta)
{
    static const bool s_dedicatedVisThread = CoreApi::GetCommandLineArguments()["DedicatedVisThread"].ToBool();

    if (m_scriptingService)
    {
        m_scriptingService->Update();
    }

    g_streamingManager->Update(delta);

    const uint32 slot = GetRingIndex();
    const uint32 frameCounter = GetFrameCounter();

    Array<Scene*, SceneAllocator> scenes;
    Array<View*, SceneAllocator> views;
    Array<Subsystem*, SceneAllocator> subsystems;

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

        // if (gameState.IsSimulating() || (world->GetWorldFlags() & WorldFlags::EDITOR_WORLD))
        // {

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
        // }

        if ((world->GetWorldFlags() & WorldFlags::EDITOR_WORLD))
        {
            // editor world gets rendered first
            worldsToRender.PushFront(world);
        }
        else
        {
            worldsToRender.PushBack(world);
        }
        // }
    }

    CommitActiveWorlds(worldsToRender.ToSpan());

    // Update Worlds and Systems - execution order/batching defined by component descriptors on systems.
    TaskSystem::GetInstance().EnqueueBatch(&worldUpdateTaskBatch);
    worldUpdateTaskBatch.AwaitCompletion();

    static const auto RemoveNonUnique = []<class ArrayType>(ArrayType& elems)
    {
        for (SizeType idx = 0; idx < elems.Size();)
        {
            if (elems.Find(elems[idx]) != elems.begin() + idx)
            {
                elems.Erase(elems.begin() + idx);

                continue;
            }

            ++idx;
        }
    };

    RemoveNonUnique(views);
    RemoveNonUnique(subsystems);
    RemoveNonUnique(scenes);

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

        for (View* view : views)
        {
            for (Scene* scene : view->GetScenes())
            {
                if (visitedScenes.Contains(scene))
                {
                    continue;
                }

                UpdateDirtyMeshEntities(scene, updatedEntities[Bucket_RenderProxy]);

                visitedScenes.PushBack(scene);
            }
        }
    }

#if HYP_PROCESS_SUBSYSTEMS_ASYNC
    Array<Task<void>, SceneTempAllocator> updateSubsystemTasks;

    for (Subsystem* subsystem : subsystems)
    {
        if (subsystem->GetUpdatePhase() != SubsystemUpdatePhase::BeforeVis || subsystem->RequiresUpdateOnSimThread())
        {
            continue;
        }

        subsystem->PreUpdate(delta);

        updateSubsystemTasks.PushBack(TaskSystem::GetInstance().Enqueue([subsystem, delta]
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

    g_visThreadInstance->OnFrameEnd(updatedEntities[Bucket_Visibility]);

    if (updatedEntities[Bucket_RenderProxy].Any() || updatedEntities[Bucket_Visibility].Any())
    { // remove tags for updates that were applied
        for (Scene* scene : scenes)
        {
            scene->GetEntityManager()->Unlock();

            // add pending before we mutate entity sets via removing tags.
            // if we don't, then the states of the pending entity sets may become stale
            // (they may still assume the tag components exist)
            scene->GetEntityManager()->AddPendingEntitySets();
        }

        for (Entity* entity : updatedEntities[Bucket_RenderProxy])
            entity->RemoveTag<EntityTag::UpdateRenderProxy>();

        for (Entity* entity : updatedEntities[Bucket_Visibility])
            entity->RemoveTag<EntityTag::UpdateVisibility>();

        // relock
        for (Scene* scene : scenes)
            scene->GetEntityManager()->Lock();
    }

    for (uint32 index = 0; index < views.Size(); index++)
    {
        HYP_NAMED_SCOPE("Per-view entity collection");

        View* view = views[index];
        Assert(view != nullptr);

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

    for (uint32 index = 0; index < views.Size(); index++)
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
}

#pragma endregion EngineDriver

static struct GlobalDescriptorSetsDeclarations
{
    GlobalDescriptorSetsDeclarations()
    {
#include <rendering/inl/DescriptorSets.inl>
    }
} s_globalDescriptorSetsDeclarations;

} // namespace Hyperion
