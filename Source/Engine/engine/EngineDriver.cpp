/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineGlobals.hpp>
#include <engine/EngineStats.hpp>
#include <engine/EngineMemory.hpp>
#include <engine/CVarManager.hpp>
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

#include <rendering/util/DeletionQueue.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/EntityManager.hpp>
#include <scene/Subsystem.hpp>
#include <scene/InstancedMeshProxy.hpp>

#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/MeshComponent.hpp>

#include <Core/filesystem/FsUtil.hpp>

#include <Core/debug/StackDump.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <Core/reflection/Enum.hpp> // For EnumValue()

#include <Core/cli/CommandLine.hpp>

#include <Core/net/NetRequestThread.hpp>

#include <Core/threading/Threads.hpp>
#include <Core/threading/TaskSystem.hpp>

#include <Core/Core.hpp>

#include <asset/Assets.hpp>

#include <streaming/StreamingManager.hpp>

#include <util/MeshBuilder.hpp>

#include <input/Event.hpp>

#include <system/AppContext.hpp>
#include <system/DirectoryInitializer.hpp>

#include <scripting/ScriptingService.hpp>

#include <HyperionEngine.hpp>

#define HYP_PROCESS_VIEWS_ASYNC 1
#define HYP_PROCESS_SUBSYSTEMS_ASYNC 1

#include <EngineDriver.generated.inl>

namespace Hyperion {

void HandleSignal(int signum);

EngineStatTimer g_statRenderUpdate("Render/Update");

ThreadSignal g_renderInitSignal { 0 };

void HandleSignal(int signum)
{
#ifdef HYP_WINDOWS
    Win32_CleanupWindowClasses();
#endif

    exit(signum);
}

static const FilePath& GetScriptsSourceDirectory()
{
    static DirectoryInitializer<HYP_STATIC_STRING("Source/Scripts"), /* RelativeToExecutablePath */ false> s_directory;
    return s_directory.path;
}

namespace MeshEntityHelpers {

#if 0
template <class AllocatorType>
static void UpdateInstancedMeshEntities(Scene* scene, Array<Entity*, AllocatorType>& outUpdatedEntities)
{
    EntityManager* entityManager = scene->GetEntityManager();
    AssertDebug(entityManager != nullptr);

    for (auto [entity, meshComponent, _] : entityManager->GetEntitySet<MeshComponent, TagComponent<EntityTag::UpdateInstancedMeshData>>())
    {
        Array<InstancedMeshProxy*, SceneAllocator> instancedMeshProxies;

        for (const Handle<Node>& childNode : entity->GetChildren())
        {
            if (childNode->IsA(InstancedMeshProxy::StaticClass()))
            {
                instancedMeshProxies.PushBack(static_cast<InstancedMeshProxy*>(childNode.Get()));
            }
        }

        meshComponent.numInstances = uint32(instancedMeshProxies.Size());

        if (!meshComponent.enableAutoInstancing && !meshComponent.numInstances)
        {
            continue;
        }

        if (!meshComponent.instanceData.IsValid())
        {
            Handle<InstancedMeshData> imd = MakeHandle<InstancedMeshData>(NAME_FMT("IMD_{}", entity->GetName()));

            Result registerResult = imd->Register("$Memory/Objects/Types/InstancedMeshData", AddAssetConflictMode::GenerateNewName);

            if (registerResult.HasError())
            {
                HYP_LOG(Scene, Error, "Failed to register InstancedMeshData: {}", registerResult.GetError().GetMessage());
            }

            meshComponent.instanceData = AssetReference(imd);
        }

        const Handle<InstancedMeshData>& imd = ObjCast<InstancedMeshData>(meshComponent.instanceData.Resolve());

        if (!imd.IsValid())
        {
            continue;
        }

        auto scope = imd->GetWriteScope();

        Array<Mat4f, SceneAllocator> transforms;
        transforms.Resize(instancedMeshProxies.Size());

        Array<Mat4f, SceneAllocator> previousTransforms;
        previousTransforms.Resize(instancedMeshProxies.Size());

        for (size_t i = 0; i < instancedMeshProxies.Size(); i++)
        {
            InstancedMeshProxy* imp = instancedMeshProxies[i];

            transforms[i] = imp->GetWorldMatrix(); // imp->GetLocalTransform().GetMatrix();
            previousTransforms[i] = imp->prevTransformMatrix;

            imp->prevTransformMatrix = transforms[i];
        }

        // Update transforms etc. based on the InstancedMeshProxy child objects
        imd->SetBufferData(0, transforms.Data(), transforms.Size());
        imd->SetBufferData(1, previousTransforms.Data(), previousTransforms.Size());

        entity->SetNeedsRenderProxyUpdate();

        outUpdatedEntities.PushBack(entity);
    }
}
#endif

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
      m_isShuttingDown(0)
{
}

EngineDriver::~EngineDriver()
{
}

HYP_API void EngineDriver::Init()
{
    AssertOnThread(g_mainThread);

#if HYP_EDITOR
    // Create script compilation service
    m_scriptingService = MakeUnique<ScriptingService>(
        GetScriptsSourceDirectory(),
        GetTempDirectory() / "ScriptProjects",
        CoreApi::GetExecutablePath()); // copy script binaries into executable path

    m_scriptingService->Start();
#endif

    RC<NetRequestThread> netRequestThread = MakeRefCountedPtr<NetRequestThread>();
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
    m_viewCollectionBatch->pool = &TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_GENERIC);

    SetReady(true);
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

void EngineDriver::SetDefaultWorld(const Handle<World>& defaultWorld)
{
    AssertOnThread(g_simThread);

    m_defaultWorld = defaultWorld;

    if (IsInitCalled())
    {
        InitObject(m_defaultWorld);
    }
}

void EngineDriver::AddWorld(const Handle<World>& world)
{
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

    return g_mainThreadInstance->Start();
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
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    Assert(AtomicAdd(&m_isShuttingDown, 0) >= 1);

    HYP_LOG(Engine, Info, "Stopping all engine processes");

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

    m_worlds.Clear();

    m_isShuttingDown = 0;
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

            if (!(view->GetFlags() & (ViewFlags::SHADOW_VIEW | ViewFlags::BAKER_VIEW | ViewFlags::UI_VIEW)))
            {
                view->PrepareShadowViews(views);
            }
        }
    }

    static const auto RemoveNonUnique = []<class ArrayType>(ArrayType& elems)
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

    Array<Entity*, SceneAllocator> updatedEntities[Bucket_Max];

    {
        // update mark render proxies as needing update for all entities that could be visible,
        // if they have the UpdateRenderProxy tag
        Array<Scene*, InlineAllocator<8, SceneAllocator>> visitedScenes;

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

    if (std::any_of(std::begin(updatedEntities), std::end(updatedEntities), [](const auto& arr) { return arr.Any(); }))
    {
        for (Scene* scene : scenes)
        {
            scene->GetEntityManager()->Unlock();

            // add pending before we mutate entity sets via removing tags.
            // if we don't, then the states of the pending entity sets may become stale
            // (they may still assume the tag components exist)
            scene->GetEntityManager()->AddPendingEntitySets();
        }
        
        // remove tags for updates that were applied

        for (Entity* entity : updatedEntities[Bucket_RenderProxy])
            entity->RemoveTag<EntityTag::UpdateRenderProxy>();

        for (Entity* entity : updatedEntities[Bucket_Visibility])
            entity->RemoveTag<EntityTag::UpdateVisibility>();

        // relock
        for (Scene* scene : scenes)
            scene->GetEntityManager()->Lock();
    }

    for (size_t viewIndex = 0; viewIndex < views.Size(); viewIndex++)
    {
        HYP_NAMED_SCOPE("Per-view entity collection");

        View* view = views[viewIndex];
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

    m_viewsPerFrame[slot] = Array<View*>(views);
}

#pragma endregion EngineDriver

} // namespace Hyperion
