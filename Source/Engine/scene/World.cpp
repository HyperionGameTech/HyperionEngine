/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <scene/View.hpp>
#include <scene/EntityManager.hpp>
#include <scene/EntityTag.hpp>
#include <scene/SystemExecutionGroup.hpp>
#include <scene/Subsystem.hpp>

#include <scene/systems/VisibilityStateUpdaterSystem.hpp>
#include <scene/systems/LightmapSystem.hpp>
#include <scene/systems/AnimationSystem.hpp>
#include <scene/systems/AudioSystem.hpp>
#include <scene/systems/PhysicsSystem.hpp>
#include <scene/systems/ScriptSystem.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <scene/world_grid/WorldGrid.hpp>

#include <Core/threading/Threads.hpp>
#include <Core/threading/Task.hpp>
#include <Core/threading/TaskSystem.hpp>
#include <Core/threading/DataRaceDetector.hpp>

#include <Core/config/Config.hpp>

#include <system/AppContext.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <engine/Game.hpp>

#include <engine/EngineDriver.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>

#include <physics/PhysicsWorld.hpp>

#include <World.generated.inl>

namespace Hyperion {

#define HYP_WORLD_ASYNC_SUBSYSTEM_UPDATES
#define HYP_WORLD_ASYNC_VIEW_COLLECTION

#define HYP_SYSTEMS_PARALLEL_EXECUTION
// #define HYP_SYSTEMS_LAG_SPIKE_DETECTION
// #define HYP_SYSTEM_LOG_PERFORMANCE

// if the number of systems in a group is less than this value, they will be executed sequentially
static constexpr double SystemExecutionGroupLagSpikeThreshold = 50.0;

namespace CoreApi {
extern const GlobalConfig& GetGlobalConfig();
} // namespace CoreApi

static const Name s_nameStreamingLayerScenes = NAME("Scenes_Layer");
static const Name s_nameUnnamedWorld = NAME("<unnamed world>");

World::World()
    : World(s_nameUnnamedWorld)
{
}

World::World(Name name, EnumFlags<WorldFlags> worldFlags)
    : ObjectBase(),
      m_name(name),
      m_gameInstance(nullptr),
      m_worldFlags(worldFlags),
      m_rayTracingView(nullptr),
      m_rootSynchronousExecutionGroup(nullptr)
{
    if (m_worldFlags & WorldFlags::ALL_STREAMING_LAYER_FLAGS)
    {
        Assert(m_worldFlags & WorldFlags::HAS_STREAMING, "Streaming layers require streaming to be enabled!");
    }

    // set m_viewsPerFrame to initial size. It uses fixed allocator so it won't dynamically allocate any memory anyway
    m_viewsPerFrame.Resize(m_viewsPerFrame.Capacity());
    AssertDebug(m_viewsPerFrame.Size() == RingBufferDepth);
}

World::~World()
{
    if (IsInitCalled())
    {
        Array<Handle<Scene>> scenes = std::move(m_scenes);

        for (const Handle<Scene>& scene : scenes)
        {
            if (!scene)
            {
                continue;
            }

            scene->SetOwnerThreadId(CurrentThreadId());

            OnSceneRemoved(this, scene);

            for (Subsystem* subsystem : m_subsystemsArray)
            {
                subsystem->OnSceneDetached(scene);
            }

            if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
            {
                for (const Handle<View>& view : m_views)
                {
                    if (!(view->GetFlags() & ViewFlags::ALL_WORLD_SCENES))
                    {
                        continue;
                    }

                    view->RemoveScene(scene);
                }
            }

            scene->SetWorld(nullptr);
        }
        
        for (const Handle<SystemBase>& system : m_systems)
        {
            system->OnRemovedFromWorld(this);
            system->m_world = nullptr;
        }
    }

    m_systems.Clear();

    for (SystemExecutionGroup* executionGroup : m_systemExecutionGroups)
    {
        PoolDelete(*g_scenePool, executionGroup);
    }
    m_systemExecutionGroups.Clear();

    m_rayTracingView = nullptr;

    EnqueueDeletion(std::move(m_scenes));
    EnqueueDeletion(std::move(m_views));

    for (Subsystem* subsystem : m_subsystemsArray)
    {
        subsystem->OnRemovedFromWorld();
    }

    if (m_physicsWorld)
    {
        m_physicsWorld->Teardown();
        m_physicsWorld.Reset();
    }

    if (m_worldGrid)
    {
        m_worldGrid->Shutdown();
    }
}

void World::Init()
{
    HYP_SCOPE;

    if (m_worldFlags & WorldFlags::HAS_STREAMING)
    {
        if (!m_worldGrid)
        {
            m_worldGrid = MakeHandle<WorldGrid>(this);
        }

        InitObject(m_worldGrid);
    }

    for (auto& it : m_subsystems)
    {
        const Handle<Subsystem>& subsystem = it.second;
        AssertDebug(subsystem != nullptr);

        InitObject(subsystem);

        subsystem->OnAddedToWorld();
    }
    
    // Create a View that is intended to collect objects used by RT gi/reflections
    // since we'll need to have resources bound even if they aren't directly in any camera's view frustum.
    // (for example there could be some stuff behind the player we want to see reflections of)
    if (CoreApi::GetGlobalConfig().Get("Rendering.RayTracing.Enabled").ToBool(false))
    {
        // dummy output target
        RenderTargetDesc renderTargetDesc;
        renderTargetDesc.extent = Vec2u::One();
        renderTargetDesc.attachments[0] = { TextureType::Texture2D, TextureFormat::R8 };
        renderTargetDesc.numAttachments = 1;

        Handle<Camera> camera = MakeHandle<Camera>();
        camera->SetName(NAME("RayTracingViewDummyCamera"));

        const ViewDesc rayTracingViewDesc {
            .flags = ViewFlags::RAY_TRACING | ViewFlags::NO_DRAW_CALLS
                | ViewFlags::ALL_WORLD_SCENES | ViewFlags::COLLECT_ALL_ENTITIES
                | ViewFlags::NO_FRUSTUM_CULLING,
            .renderTargetDesc = renderTargetDesc,
            .camera = camera
        };

        Handle<View> rayTracingView = MakeHandle<View>(rayTracingViewDesc);
        InitObject(rayTracingView);

        m_rayTracingView = rayTracingView;

        m_views.PushBack(std::move(rayTracingView));
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        scene->SetWorld(this);

        InitObject(scene);

        OnSceneAdded(this, scene);

        for (Subsystem* subsystem : m_subsystemsArray)
        {
            subsystem->OnSceneAttached(scene);
        }

        if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
        {
            for (const Handle<View>& view : m_views)
            {
                if (!(view->GetFlags() & ViewFlags::ALL_WORLD_SCENES))
                {
                    continue;
                }

                view->AddScene(scene);
            }
        }

        // add to streaming layer if applicable
        if ((m_worldFlags & WorldFlags::HAS_SCENE_STREAMING_LAYER) && (scene->GetSceneFlags() & SceneFlags::STREAMED))
        {
            Handle<WorldGridLayer> scenesStreamingLayer = GetOrCreateStreamingLayer(s_nameStreamingLayerScenes);
            AssertDebug(scenesStreamingLayer != nullptr);
            scenesStreamingLayer->AddStreamingObject(scene, scene->GetStreamingCentroid());
        }
    }
    
    for (const Handle<SystemBase>& system : m_systems)
    {
        AssertDebug(system != nullptr);

        if (!system)
        {
            continue;
        }

        system->InitComponentInfos_Internal();

        const bool wasAddedToExecutionGroup = AddSystemToExecutionGroup(system);

        system->m_world = this;

        InitObject(system);
        
        system->OnAddedToWorld(this);

        if (wasAddedToExecutionGroup)
        {
            for (const Handle<Scene>& scene : m_scenes)
            {
                if (scene != nullptr)
                {
                    scene->GetEntityManager()->NotifySystemOfExistingEntities(system);
                }
            }
        }
    }

    if (!HasSystem<VisibilityStateUpdaterSystem>())
        AddSystem(MakeHandle<VisibilityStateUpdaterSystem>());
    
    if (!HasSystem<LightmapSystem>())
        AddSystem(MakeHandle<LightmapSystem>());
    
    if (!HasSystem<AnimationSystem>())
        AddSystem(MakeHandle<AnimationSystem>());
    
    if (!HasSystem<AudioSystem>())
        AddSystem(MakeHandle<AudioSystem>());
    
    if (!HasSystem<PhysicsSystem>())
        AddSystem(MakeHandle<PhysicsSystem>());

    if (!HasSystem<ScriptSystem>())
        AddSystem(MakeHandle<ScriptSystem>());

    for (const Handle<View>& view : m_views)
    {
        if (view->m_rayTracingView.GetUnsafe() != m_rayTracingView)
        {
            if (view->m_rayTracingView)
            {
                HYP_LOG(Scene, Warning,
                    "View {} already has a rayTracing View set! Was it added to multiple Worlds with rayTracing enabled?",
                    view->Id());

                view->m_rayTracingView.Reset();
            }

            if (m_rayTracingView != nullptr)
            {
                view->m_rayTracingView = m_rayTracingView->WeakHandleFromThis();
            }
        }

        InitObject(view);
    }

    if (m_worldFlags & WorldFlags::HAS_PHYSICS)
    {
        m_physicsWorld = MakeHandle<PhysicsWorld>();
        InitObject(m_physicsWorld);
    }

    ObjectBase::Init();

    SetReady(true);
}

void World::SetWorldFlags(EnumFlags<WorldFlags> flags)
{
    uint32 changedFlags = uint32(m_worldFlags) ^ uint32(flags);

    if (changedFlags == 0)
    {
        return;
    }

    m_worldFlags = flags;

    const bool isInitialized = IsInitCalled();

    if (isInitialized)
    {
        if (changedFlags & uint32(WorldFlags::HAS_PHYSICS))
        {
            if (m_worldFlags & WorldFlags::HAS_PHYSICS)
            {
                m_physicsWorld = MakeHandle<PhysicsWorld>();
                InitObject(m_physicsWorld);
            }
            else
            {
                if (m_physicsWorld)
                {
                    m_physicsWorld->Teardown();
                    m_physicsWorld.Reset();
                }
            }
        }

        bool needToShutdownWorldGrid = false;

        if (changedFlags & uint32(WorldFlags::HAS_STREAMING))
        {
            if (m_worldFlags & WorldFlags::HAS_STREAMING)
            {
                if (!m_worldGrid)
                {
                    m_worldGrid = MakeHandle<WorldGrid>(this);
                    InitObject(m_worldGrid);
                }
            }
            else
            {
                if (m_worldGrid)
                {
                    needToShutdownWorldGrid = true;

                    // have to turn off all streaming layers
                    const EnumFlags<WorldFlags> streamingLayerFlagsBefore = m_worldFlags & WorldFlags::ALL_STREAMING_LAYER_FLAGS;
                    m_worldFlags &= ~WorldFlags::ALL_STREAMING_LAYER_FLAGS;

                    // add changed flags for all streaming layers that were on before -  we handle them below
                    changedFlags |= uint32(m_worldFlags & WorldFlags::ALL_STREAMING_LAYER_FLAGS) ^ uint32(streamingLayerFlagsBefore);
                }
            }
        }

        if (changedFlags & uint32(WorldFlags::HAS_SCENE_STREAMING_LAYER))
        {
            if (m_worldFlags & WorldFlags::HAS_SCENE_STREAMING_LAYER)
            {
                Handle<WorldGridLayer> scenesStreamingLayer = GetOrCreateStreamingLayer(s_nameStreamingLayerScenes);
                AssertDebug(scenesStreamingLayer != nullptr);

                for (const Handle<Scene>& scene : m_scenes)
                {
                    if (!(scene->GetSceneFlags() & SceneFlags::STREAMED))
                    {
                        continue;
                    }

                    scenesStreamingLayer->AddStreamingObject(scene, scene->GetStreamingCentroid());
                }
            }
            else
            {
                /// \todo Need to load all scenes from the streaming layer into the world before removing it
            }
        }

        // shutdown after disabling layers
        if (needToShutdownWorldGrid)
        {
            m_worldGrid->Shutdown();
            m_worldGrid.Reset();
        }
    }
}

void World::SetFogParams(const FogParams& fogParams)
{
    m_fogParams = fogParams;
}

void World::SetCSMParams(const CSMParams& csmParams)
{
    m_csmParams = csmParams;
}

const GameState& World::GetGameState() const
{
    if (m_gameInstance != nullptr)
    {
        return m_gameInstance->GetGameState();
    }

    // fallback
    static GameState s_defaultGameState;
    return s_defaultGameState;
}

void World::ProcessViewAsync(View* view)
{
    if (!view)
    {
        return;
    }

    if (m_processViews.Contains(view))
    {
        return;
    }

    m_processViews.PushBack(view);
}

void World::BeginUpdate(TaskBatch& inBatch, float delta)
{
    HYP_SCOPE;

    UpdateCSMState();

    for (Scene* scene : m_scenes)
    {
        scene->Update(delta);

        scene->GetEntityManager()->Lock();
    }

    if (m_physicsWorld != nullptr)
    {
        m_physicsWorld->Tick(delta);
    }

    m_rootSynchronousExecutionGroup = nullptr;

    TaskBatch* firstTaskBatch = nullptr;
    TaskBatch* lastTaskBatch = nullptr;

    // Prepare task dependencies
    for (size_t index = 0; index < m_systemExecutionGroups.Size(); index++)
    {
        SystemExecutionGroup& systemExecutionGroup = *m_systemExecutionGroups[index];

        if (!systemExecutionGroup.AllowUpdate())
        {
            continue;
        }

        TaskBatch* currentTaskBatch = systemExecutionGroup.GetTaskBatch();
        AssertDebug(currentTaskBatch != nullptr);

        AssertDebug(currentTaskBatch->IsCompleted(), "TaskBatch for SystemExecutionGroup is not completed: {} tasks enqueued", currentTaskBatch->numEnqueued);
        currentTaskBatch->ResetState();

        // Add tasks to batches before kickoff
        systemExecutionGroup.StartProcessing(delta, m_scenes.ToSpan());

        if (systemExecutionGroup.RequiresSimThread())
        {
            if (m_rootSynchronousExecutionGroup != nullptr)
            {
                m_rootSynchronousExecutionGroup->GetTaskBatch()->nextBatch = currentTaskBatch;
            }
            else
            {
                m_rootSynchronousExecutionGroup = &systemExecutionGroup;
            }

            continue;
        }

        if (!firstTaskBatch)
        {
            firstTaskBatch = currentTaskBatch;
        }

        if (lastTaskBatch != nullptr)
        {
            if (currentTaskBatch->executors.Any())
            {
                lastTaskBatch->nextBatch = currentTaskBatch;
                lastTaskBatch = currentTaskBatch;
            }
        }
        else
        {
            lastTaskBatch = currentTaskBatch;
        }
    }

    // Kickoff first task
    if (firstTaskBatch != nullptr)
    {
        AssertDebug(inBatch.nextBatch == nullptr);

        if (firstTaskBatch->executors.Any())
        {
            inBatch.nextBatch = firstTaskBatch;
        }
        else if (firstTaskBatch->nextBatch != nullptr)
        {
            inBatch.nextBatch = firstTaskBatch->nextBatch;
        }
    }
}

void World::EndUpdate()
{
    HYP_SCOPE;

    for (SystemExecutionGroup* systemExecutionGroup : m_systemExecutionGroups)
    {
        if (!systemExecutionGroup->AllowUpdate() || systemExecutionGroup->RequiresSimThread())
        {
            continue;
        }

        systemExecutionGroup->FinishProcessing();
    }

    if (m_rootSynchronousExecutionGroup != nullptr)
    {
        m_rootSynchronousExecutionGroup->FinishProcessing(/* executeBlocking */ true);

        m_rootSynchronousExecutionGroup = nullptr;
    }

#if defined(HYP_DEBUG_MODE) && (defined(HYP_SYSTEM_LOG_PERFORMANCE) || defined(HYP_SYSTEMS_LAG_SPIKE_DETECTION))
    for (SystemExecutionGroup& systemExecutionGroup : m_systemExecutionGroups)
    {
        const PerformanceClock& performanceClock = systemExecutionGroup.GetPerformanceClock();
        const double elapsedTimeMs = performanceClock.ElapsedMs();

#ifdef HYP_SYSTEMS_LAG_SPIKE_DETECTION
        if (elapsedTimeMs >= SystemExecutionGroupLagSpikeThreshold)
        {
            HYP_LOG(Entity, Warning, "SystemExecutionGroup spike detected: {} ms", elapsedTimeMs);
        }
#endif
#ifdef HYP_SYSTEM_LOG_PERFORMANCE
        for (const auto& it : systemExecutionGroup.GetPerformanceClocks())
        {
            HYP_LOG(Entity, Verbose, "\tSystem {} performance: {}", it.first->GetName(), it.second.ElapsedMs());
        }
#endif
    }
#endif
}

void World::UpdateCSMState()
{
    const GameState& gameState = GetGameState();

    if (gameState.IsSimulating())
    {
        bool found = false;

        for (Scene* scene : m_scenes)
        {
            for (auto [entity, _0, _1] : scene->GetEntityManager()->GetEntitySet<EntityType<Camera>, TagComponent<EntityTag::PrimaryCamera>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
            {
                Camera* camera = static_cast<Camera*>(entity);

                found = true;

                m_csmState.playerCenter = camera->GetTranslation();

                break;
            }

            if (found)
            {
                break;
            }
        }
    }
#if HYP_EDITOR
    else if (gameState.IsEditMode())
    {
        bool found = false;

        for (Scene* scene : m_scenes)
        {
            for (auto [entity, _0, _1] : scene->GetEntityManager()->GetEntitySet<EntityType<Camera>, TagComponent<EntityTag::EditorCamera>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
            {
                Camera* camera = static_cast<Camera*>(entity);

                found = true;

                m_csmState.playerCenter = camera->GetTranslation();

                break;
            }

            if (found)
            {
                break;
            }
        }
    }
#endif
}

void World::CollectScenes(Array<Scene*, SceneTempAllocator>& outScenes)
{
    outScenes.Reserve(outScenes.Size() + m_scenes.Size());

    for (Scene* scene : m_scenes)
    {
        outScenes.PushBack(scene);
    }
}

void World::CollectCameras(Array<Camera*, SceneTempAllocator>& outCameras)
{
    outCameras.Reserve(m_scenes.Size() * 3);

    for (Scene* scene : m_scenes)
    {
        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<Camera>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            outCameras.PushBack(static_cast<Camera*>(entity));
        }
    }
}

void World::CollectViews(Array<View*, SceneTempAllocator>& outViews)
{
    const uint32 slot = GetRingIndex();

    m_viewsPerFrame[slot].Resize(m_views.Size() + m_processViews.Size());

    if (m_views.Empty() && m_processViews.Empty())
    {
        return;
    }

    { // set buffered Views for current frame index
        for (size_t i = 0; i < m_views.Size(); i++)
        {
            m_viewsPerFrame[slot][i] = m_views[i].Get();
        }

        const size_t offset = m_views.Size();
        for (size_t i = 0; i < m_processViews.Size(); i++)
        {
            m_viewsPerFrame[slot][offset + i] = m_processViews[i];
        }
    }

    { // add all views to outViews
        size_t offset = outViews.Size();
        outViews.Resize(offset + m_processViews.Size() + m_views.Size());

        for (size_t i = 0; i < m_views.Size(); i++)
        {
            AssertDebug(m_views[i] != nullptr);
            AssertDebug(!m_processViews.Contains(m_views[i]));

            outViews[offset + i] = m_views[i].Get();
        }

        offset += m_views.Size();

        for (size_t i = 0; i < m_processViews.Size(); i++)
        {
            outViews[offset + i] = m_processViews[i];
        }
    }

    // Clear additional Views to process for next frame
    m_processViews.Clear();
}

void World::CollectSubsystems(Array<Subsystem*, SceneTempAllocator>& outSubsystems)
{
    const size_t offset = outSubsystems.Size();
    outSubsystems.Resize(offset + m_subsystemsArray.Size());

    for (size_t i = 0; i < m_subsystemsArray.Size(); i++)
    {
        outSubsystems[offset + i] = m_subsystemsArray[i];
    }
}

const Handle<Subsystem>& World::AddSubsystem(TypeId typeId, const Handle<Subsystem>& subsystem)
{
    HYP_SCOPE;

    if (!subsystem)
    {
        return Handle<Subsystem>::Null();
    }

    const auto it = m_subsystems.Find(typeId);

    if (it != m_subsystems.End())
    {
        HYP_LOG(Scene, Warning, "Attempting to add Subsystem of type {}, but one already exists on World {}!", *subsystem->InstanceClass()->GetName(), GetName());

        return it->second;
    }

    subsystem->SetWorld(this);

    auto insertResult = m_subsystems.Set(typeId, subsystem);
    Assert(insertResult.second);

    // fine to take reference since we use dynamic hash map allocator which doesn't invalidate references.
    const Handle<Subsystem>& newSubsystem = insertResult.first->second;
    AssertDebug(newSubsystem != nullptr);

    m_subsystemsArray.PushBack(newSubsystem.Get());

    // If World is already initialized, initialize the subsystem
    // otherwise, it will be initialized when World::Init() is called
    if (IsInitCalled())
    {
        if (insertResult.second)
        {
            InitObject(newSubsystem);

            newSubsystem->OnAddedToWorld();

            for (Handle<Scene>& scene : m_scenes)
            {
                newSubsystem->OnSceneAttached(scene);
            }
        }
    }

    return newSubsystem;
}

bool World::TryAddSubsystem(const Handle<Subsystem>& subsystem)
{
    HYP_SCOPE;

    if (!subsystem)
    {
        return false;
    }

    Handle<Subsystem> result = AddSubsystem(subsystem->InstanceClass()->GetTypeId(), subsystem);

    if (!result || result.Get() != subsystem.Get())
    {
        return false;
    }

    return true;
}

Subsystem* World::GetSubsystem(TypeId typeId) const
{
    HYP_SCOPE;

    const auto it = m_subsystems.Find(typeId);

    if (it == m_subsystems.End())
    {
        return nullptr;
    }

    return it->second.Get();
}

Subsystem* World::GetSubsystemByName(StringHash name) const
{
    HYP_SCOPE;

    const auto it = m_subsystemsArray.FindIf([name](Subsystem* subsystem)
        {
            const Class* cls = subsystem->InstanceClass();

            return cls->GetName() == name;
        });

    if (it == m_subsystemsArray.End())
    {
        return nullptr;
    }

    return *it;
}

bool World::RemoveSubsystem(Subsystem* subsystem)
{
    HYP_SCOPE;

    if (!subsystem)
    {
        return false;
    }

    const TypeId typeId = subsystem->InstanceClass()->GetTypeId();

    auto it = m_subsystems.Find(typeId);

    if (it == m_subsystems.End())
    {
        return false;
    }

    Assert(it->second.Get() == subsystem);

    if (IsInitCalled())
    {
        for (const Handle<Scene>& scene : m_scenes)
        {
            if (!scene)
            {
                continue;
            }

            subsystem->OnSceneDetached(scene);
        }

        subsystem->OnRemovedFromWorld();
    }

    subsystem->SetWorld(nullptr);

    m_subsystems.Erase(it);

    auto arrayIt = m_subsystemsArray.Find(subsystem);
    Assert(arrayIt != m_subsystemsArray.End());

    m_subsystemsArray.Erase(arrayIt);

    return true;
}

void World::AddScene(const Handle<Scene>& scene, bool addToStreamingLayer)
{
    HYP_SCOPE;

    if (!scene)
    {
        return;
    }

    if (m_scenes.Contains(scene))
    {
        HYP_LOG(Scene, Warning, "Scene {} already exists in world", scene->GetName());

        return;
    }

    if (addToStreamingLayer)
    {
        if (!(scene->GetSceneFlags() & SceneFlags::STREAMED))
        {
            HYP_LOG(Scene, Warning,
                "Adding Scene {} to World {}'s streaming layer, but the Scene is not marked as STREAMED! ",
                scene->GetName(),
                GetName());

            addToStreamingLayer = false;
        }
    }

    scene->SetWorld(this);

    if (IsInitCalled())
    {
        InitObject(scene);

        OnSceneAdded(this, scene);

        for (Subsystem* subsystem : m_subsystemsArray)
        {
            subsystem->OnSceneAttached(scene);
        }

        if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
        {
            for (const Handle<View>& view : m_views)
            {
                if (!(view->GetFlags() & ViewFlags::ALL_WORLD_SCENES))
                {
                    continue;
                }

                view->AddScene(scene);
            }
        }

        if (addToStreamingLayer && (m_worldFlags & WorldFlags::HAS_SCENE_STREAMING_LAYER) && (scene->GetSceneFlags() & SceneFlags::STREAMED))
        {
            Handle<WorldGridLayer> scenesStreamingLayer = GetOrCreateStreamingLayer(s_nameStreamingLayerScenes);
            AssertDebug(scenesStreamingLayer != nullptr);
            scenesStreamingLayer->AddStreamingObject(scene, scene->GetStreamingCentroid());
        }
    }

    m_scenes.PushBack(scene);
}

bool World::RemoveScene(Scene* scene, bool removeFromStreamingLayer)
{
    HYP_SCOPE;

    auto it = m_scenes.Find(scene);

    if (it == m_scenes.End())
    {
        return false;
    }

    Handle<Scene> strongScene = std::move(*it);

    m_scenes.Erase(it);

    if (scene != nullptr)
    {
        scene->SetWorld(nullptr);

        if (IsInitCalled())
        {
            if (removeFromStreamingLayer && (m_worldFlags & WorldFlags::HAS_SCENE_STREAMING_LAYER))
            {
                Handle<WorldGridLayer> scenesStreamingLayer = GetOrCreateStreamingLayer(s_nameStreamingLayerScenes);
                AssertDebug(scenesStreamingLayer != nullptr);
                scenesStreamingLayer->RemoveStreamingObject(scene);
            }

            OnSceneRemoved(this, scene);

            for (Subsystem* subsystem : m_subsystemsArray)
            {
                subsystem->OnSceneDetached(scene);
            }

            for (const Handle<View>& view : m_views)
            {
                view->RemoveScene(scene);
            }
        }
    }

    EnqueueDeletion(std::move(strongScene));

    return true;
}

bool World::HasScene(ObjId<Scene> sceneId) const
{
    return m_scenes.FindIf([sceneId](const Handle<Scene>& scene)
               {
                   return scene.Id() == sceneId;
               })
        != m_scenes.End();
}

const Handle<Scene>& World::GetSceneByName(Name name) const
{
    const auto it = m_scenes.FindIf([name](const Handle<Scene>& scene)
        {
            return scene->GetName() == name;
        });

    return it != m_scenes.End() ? *it : Handle<Scene>::empty;
}

void World::AddView(const Handle<View>& view)
{
    HYP_SCOPE;

    if (!view)
    {
        return;
    }

    const bool hasView = m_views.Contains(view);
    AssertDebug(!hasView, "World {} already contains view {}", GetName(), view->Id());

    if (hasView)
    {
        return;
    }

    m_views.PushBack(view);

    if (IsInitCalled())
    {
        if (view->m_rayTracingView.GetUnsafe() != m_rayTracingView)
        {
            if (view->m_rayTracingView)
            {
                HYP_LOG(Scene, Warning,
                    "View {} already has a rayTracing View set! Was it added to multiple Worlds with rayTracing enabled?",
                    view->Id());

                view->m_rayTracingView.Reset();
            }

            if (m_rayTracingView != nullptr)
            {
                view->m_rayTracingView = m_rayTracingView->WeakHandleFromThis();
            }
        }

        // Add all scenes to the view, if the view should collect all world scenes
        if (view->GetFlags() & ViewFlags::ALL_WORLD_SCENES)
        {
            for (const Handle<Scene>& scene : m_scenes)
            {
                if (!scene)
                {
                    continue;
                }

                if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
                {
                    view->AddScene(scene);
                }
            }
        }

        InitObject(view);
    }
}

void World::RemoveView(View* view)
{
    HYP_SCOPE;

    if (!view)
    {
        return;
    }

    if (IsInitCalled())
    {
        view->m_rayTracingView.Reset();

        // Remove all scenes from the view, if the view should collect all world scenes
        if (view->GetFlags() & ViewFlags::ALL_WORLD_SCENES)
        {
            for (const Handle<Scene>& scene : m_scenes)
            {
                if (!scene)
                {
                    continue;
                }

                view->RemoveScene(scene);
            }
        }
    }

    auto it = m_views.Find(view);

    if (it == m_views.End())
    {
        return;
    }

    Handle<View> strongView = std::move(*it);
    m_views.Erase(it);

    EnqueueDeletion(std::move(strongView));
}

Span<View* const> World::GetViews() const
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread | g_simThread);

    return m_viewsPerFrame[GetRingIndex()].ToSpan();
}

void World::DeserializeNonStreamingScenes(const Array<Handle<Scene>>& scenes)
{
    HYP_SCOPE;
    // no thread assertion if not yet init since this is used for deserialization mainly

    const bool isInitialized = IsInitCalled();

    for (Handle<Scene>& scene : m_scenes)
    {
        if (m_worldFlags & WorldFlags::HAS_SCENE_STREAMING_LAYER)
        {
            // Remove scene from streaming layer if its currently enabled
            Handle<WorldGridLayer> scenesStreamingLayer = GetOrCreateStreamingLayer(s_nameStreamingLayerScenes);
            AssertDebug(scenesStreamingLayer != nullptr);
            scenesStreamingLayer->RemoveStreamingObject(scene);
        }

        scene->SetWorld(nullptr);

        if (isInitialized)
        {
            OnSceneRemoved(this, scene);

            for (Subsystem* subsystem : m_subsystemsArray)
            {
                subsystem->OnSceneDetached(scene);
            }

            for (const Handle<View>& view : m_views)
            {
                view->RemoveScene(scene);
            }
        }

        EnqueueDeletion(std::move(scene));
    }

    m_scenes.Clear();

    for (const Handle<Scene>& scene : scenes)
    {
        if (!scene)
        {
            continue;
        }

        if (m_scenes.Contains(scene))
        {
            continue; // prevent double add
        }

        scene->SetWorld(this);

        if (isInitialized)
        {
            InitObject(scene);

            OnSceneAdded(this, scene);

            for (Subsystem* subsystem : m_subsystemsArray)
            {
                subsystem->OnSceneAttached(scene);
            }

            if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
            {
                for (const Handle<View>& view : m_views)
                {
                    if (!(view->GetFlags() & ViewFlags::ALL_WORLD_SCENES))
                    {
                        continue;
                    }

                    view->AddScene(scene);
                }
            }
        }

        m_scenes.PushBack(scene);
    }
}

Array<Handle<Scene>> World::SerializeNonStreamingScenes() const
{
    HYP_SCOPE;

    if (m_worldFlags & WorldFlags::HAS_SCENE_STREAMING_LAYER)
    {
        // return nothing if we have streaming enabled.
        return {};
    }

    return m_scenes;
}

static void BindStreamingDelegates(DelegateHandlerSet& set, World* world, WorldGridLayer* layer)
{
    AssertDebug(world != nullptr && layer != nullptr);

    set.Remove(&layer->OnStreamingObjectsLoaded);
    set.Remove(&layer->OnStreamingObjectsUnloaded);

    set.Add(layer->OnStreamingObjectsLoaded.Bind([world](StreamingCell* cell, Array<const AssetObject*> objs)
        {
            AssertOnThread(g_simThread);
            for (const AssetObject* obj : objs)
            {
                if (obj->IsA(Scene::StaticClass()))
                {
                    const Scene* scene = ObjCast<Scene>(obj);

                    world->AddScene(MakeStrongRef(scene), /* addToStreamingLayer */ false);

                    continue;
                }
            }
        }));

    set.Add(layer->OnStreamingObjectsUnloaded.Bind([world](StreamingCell* cell, Array<const AssetObject*> objs)
        {
            AssertOnThread(g_simThread);
            for (const AssetObject* obj : objs)
            {
                if (obj->IsA(Scene::StaticClass()))
                {
                    const Scene* scene = ObjCast<Scene>(obj);

                    world->RemoveScene(const_cast<Scene*>(scene), /* removeFromStreamingLayer */ false);

                    continue;
                }
            }
        }));
}

Handle<WorldGridLayer> World::GetOrCreateStreamingLayer(Name streamingLayerName)
{
    HYP_SCOPE;

    AssertDebug(streamingLayerName.IsValid());
    if (!streamingLayerName.IsValid())
    {
        return Handle<WorldGridLayer>::Null();
    }

    AssertDebug(m_worldGrid != nullptr);
    if (!m_worldGrid)
    {
        return Handle<WorldGridLayer>::Null();
    }

    auto it = m_worldGrid->GetLayers().FindIf([streamingLayerName](const Handle<WorldGridLayer>& layer)
        {
            return layer->GetName() == streamingLayerName;
        });

    if (it != m_worldGrid->GetLayers().End())
    {
        return *it;
    }

    Handle<WorldGridLayer> layer = MakeHandle<WorldGridLayer>(streamingLayerName);
    BindStreamingDelegates(m_delegateHandlers, this, layer);

    m_worldGrid->AddLayer(layer);

    return layer;
}

void World::DeserializeStreamingLayers(const Array<WGLayerDesc, DynamicAllocator>& streamingLayers)
{
    if (m_worldGrid != nullptr)
    {
        for (const Handle<WorldGridLayer>& layer : m_worldGrid->GetLayers())
        {
            m_delegateHandlers.Remove(&layer->OnStreamingObjectsLoaded);
            m_delegateHandlers.Remove(&layer->OnStreamingObjectsUnloaded);

            /// \todo remove Scenes if layer is scene streaming layer?
        }
    }
    else
    {
        if (!(m_worldFlags & WorldFlags::HAS_STREAMING))
        {
            HYP_LOG(Scene, Warning,
                "Attempted to deserialize streaming layers on World {} which does not have WorldGrid enabled!",
                GetName());

            return;
        }

        m_worldGrid = MakeHandle<WorldGrid>(this);
    }

    m_worldGrid->SetStreamingLayersFromDescs(streamingLayers.ToSpan());

    for (const Handle<WorldGridLayer>& layer : m_worldGrid->GetLayers())
    {
        BindStreamingDelegates(m_delegateHandlers, this, layer);

        /// \todo if scene streaming is enabled and we're Init()'d, stream them in!
    }

    if (IsInitCalled())
    {
        InitObject(m_worldGrid);
    }
}

Array<WGLayerDesc, DynamicAllocator> World::SerializeStreamingLayers() const
{
    AssertDebug(m_worldGrid != nullptr);

    if (!m_worldGrid)
    {
        return {};
    }

    return m_worldGrid->GetStreamingLayerDescs();
}

void World::DeserializeSystems(const Array<Handle<SystemBase>>& systems)
{
    // remove existing
    for (size_t systemIdx = 0; systemIdx < m_systems.Size();)
    {
        SystemBase* system = m_systems[systemIdx];

        if (!RemoveSystem(system))
        {
            ++systemIdx;

            continue;
        }
    }

    AssertDebug(m_systems.Empty());

    for (const Handle<SystemBase>& system : systems)
    {
        if (!system)
            continue;

        AddSystem(system);
    }
}

Array<Handle<SystemBase>> World::SerializeSystems() const
{
    Array<Handle<SystemBase>> systemsToSerialize;
    systemsToSerialize.Reserve(m_systems.Size());

    for (const Handle<SystemBase>& system : m_systems)
    {
        // Skip systems with `Serialize` attr as false
        if (!system->InstanceClass()->GetAttribute(Attributes::g_attrSerialize).GetBool(true))
            continue;

        systemsToSerialize.PushBack(system);
    }

    return systemsToSerialize;
}

SystemBase* World::AddSystem(const Handle<SystemBase>& system)
{
    Assert(system.IsValid());
    Assert(system->m_world == nullptr || system->m_world == this);

    auto it = m_systems.FindIf([&system](const Handle<SystemBase>& otherSystem)
        {
            return otherSystem->InstanceClass() == system->InstanceClass();
        });

    AssertDebug(it == m_systems.End(), "System of type {} already exists in world {}", system->InstanceClass()->GetName(), GetName());
    if (it != m_systems.End())
    {
        // cannot add system if one already exists
        return *it;
    }

    m_systems.PushBack(system);

    // If the World is initialized, call Initialize() on the System.
    if (IsInitCalled())
    {
        system->InitComponentInfos_Internal();

        const bool wasAddedToExecutionGroup = AddSystemToExecutionGroup(system);

        system->m_world = this;

        InitObject(system);
        
        system->OnAddedToWorld(this);

        if (wasAddedToExecutionGroup)
        {
            for (const Handle<Scene>& scene : m_scenes)
            {
                if (scene != nullptr)
                {
                    scene->GetEntityManager()->NotifySystemOfExistingEntities(system);
                }
            }
        }
    }

    return system;
}

bool World::RemoveSystem(SystemBase* system)
{
    HYP_SCOPE;

    if (!system)
    {
        return false;
    }

    auto it = m_systems.Find(system);

    if (it == m_systems.End())
    {
        return false;
    }

    AssertDebug(system->m_world == this);

    Handle<SystemBase> systemStrong = MakeStrongRef(system);

    m_systems.Erase(it);

    if (IsInitCalled())
    {
        SystemExecutionGroup* executionGroup = nullptr;
        for (SystemExecutionGroup* systemExecutionGroup : m_systemExecutionGroups)
        {
            if (systemExecutionGroup->HasSystem(system))
            {
                executionGroup = systemExecutionGroup;
                break;
            }
        }

        if (executionGroup != nullptr)
        {
            for (const Handle<Scene>& scene : m_scenes)
            {
                scene->GetEntityManager()->NotifySystemOfAllEntitiesRemoved(system);
            }

            const bool wasRemoved = executionGroup->RemoveSystem(system);
            AssertDebug(wasRemoved);
        }
    }
    
    systemStrong->OnRemovedFromWorld(this);
    systemStrong->m_world = nullptr;

    return true;
}

bool World::AddSystemToExecutionGroup(SystemBase* system)
{
    Assert(system != nullptr);

    bool wasAdded = false;

    if (system->AllowParallelExecution())
    {
        for (SystemExecutionGroup* systemExecutionGroup : m_systemExecutionGroups)
        {
            if (systemExecutionGroup->IsValidForSystem(system))
            {
                if (systemExecutionGroup->AddSystem(system))
                {
                    wasAdded = true;

                    break;
                }
            }
        }
    }

    if (!wasAdded)
    {
        SystemExecutionGroup*& systemExecutionGroup = m_systemExecutionGroups.EmplaceBack();
        systemExecutionGroup = PoolNew<SystemExecutionGroup>(*g_scenePool, system->RequiresSimThread(), system->AllowUpdate());

        if (systemExecutionGroup->AddSystem(system))
        {
            wasAdded = true;
        }
    }

    return wasAdded;
}

} // namespace Hyperion
