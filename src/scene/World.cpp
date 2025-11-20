/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/EntityManager.hpp>
#include <scene/EntityTag.hpp>
#include <scene/SystemExecutionGroup.hpp>

#include <scene/systems/WorldAABBUpdaterSystem.hpp>
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

#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/reflection/Class.hpp>

#include <core/utilities/Format.hpp>

#include <core/config/Config.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <system/AppContext.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>

#include <World.generated.inl>

namespace hyperion {

#define HYP_WORLD_ASYNC_SUBSYSTEM_UPDATES
#define HYP_WORLD_ASYNC_VIEW_COLLECTION

#define HYP_SYSTEMS_PARALLEL_EXECUTION
// #define HYP_SYSTEMS_LAG_SPIKE_DETECTION
// #define HYP_SYSTEM_LOG_PERFORMANCE

// if the number of systems in a group is less than this value, they will be executed sequentially
static constexpr double SystemExecutionGroupLagSpikeThreshold = 50.0;

extern const GlobalConfig& CoreApi_GetGlobalConfig();

static const Name s_nameStreamingLayerScenes = NAME("Scenes_Layer");
static const Name s_nameUnnamedWorld = NAME("<unnamed world>");

World::World()
    : World(s_nameUnnamedWorld)
{
}

World::World(Name name, EnumFlags<WorldFlags> worldFlags)
    : ObjectBase(),
      m_name(name),
      m_worldFlags(worldFlags),
      m_raytracingView(nullptr),
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
    }

    m_raytracingView = nullptr;

    SafeDelete(std::move(m_scenes));
    SafeDelete(std::move(m_views));

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
            m_worldGrid = CreateObject<WorldGrid>(this);
        }

        InitObject(m_worldGrid);
    }

    AddSystem(CreateObject<WorldAABBUpdaterSystem>());
    AddSystem(CreateObject<VisibilityStateUpdaterSystem>());
    AddSystem(CreateObject<LightmapSystem>());
    AddSystem(CreateObject<AnimationSystem>());
    AddSystem(CreateObject<AudioSystem>());
    AddSystem(CreateObject<PhysicsSystem>());
    AddSystem(CreateObject<ScriptSystem>());

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
    if (CoreApi_GetGlobalConfig().Get("Rendering.RayTracing.Enabled").ToBool(false))
    {
        // dummy output target
        ViewOutputTargetDesc outputTargetDesc {
            .extent = Vec2u::One(),
            .attachments = { { TF_R8 } }
        };

        Handle<Camera> camera = CreateObject<Camera>();

        const ViewDesc raytracingViewDesc {
            .flags = ViewFlags::RAYTRACING | ViewFlags::NO_DRAW_CALLS
                | ViewFlags::ALL_WORLD_SCENES | ViewFlags::COLLECT_ALL_ENTITIES
                | ViewFlags::NO_FRUSTUM_CULLING,
            .viewport = Viewport { .extent = Vec2u::One(), .position = Vec2i::Zero() },
            .outputTargetDesc = outputTargetDesc,
            .camera = camera
        };

        Handle<View> raytracingView = CreateObject<View>(raytracingViewDesc);
        InitObject(raytracingView);

        m_raytracingView = raytracingView;

        m_views.PushBack(std::move(raytracingView));
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

    for (const Handle<View>& view : m_views)
    {
        if (view->m_raytracingView.GetUnsafe() != m_raytracingView)
        {
            if (view->m_raytracingView)
            {
                HYP_LOG(Scene, Warning,
                    "View {} already has a raytracing View set! Was it added to multiple Worlds with raytracing enabled?",
                    view->Id());

                view->m_raytracingView.Reset();
            }

            if (m_raytracingView != nullptr)
            {
                view->m_raytracingView = m_raytracingView->WeakHandleFromThis();
            }
        }

        InitObject(view);
    }

    if (m_worldFlags & WorldFlags::HAS_PHYSICS)
    {
        m_physicsWorld = CreateObject<PhysicsWorld>();
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

    const bool isReady = IsReady();

    if (isReady)
    {
        if (changedFlags & uint32(WorldFlags::HAS_PHYSICS))
        {
            if (m_worldFlags & WorldFlags::HAS_PHYSICS)
            {
                m_physicsWorld = CreateObject<PhysicsWorld>();
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
                    m_worldGrid = CreateObject<WorldGrid>(this);
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
                // @TODO Need to load all scenes from the streaming layer into the world before removing it
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

void World::ProcessViewAsync(View* view)
{
    HYP_SCOPE;
    AssertReady();

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

    m_gameState.deltaTime = delta;

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertDebug(scene != nullptr);
        scene->Update(delta);
    }

    if (m_physicsWorld)
    {
        m_physicsWorld->Tick(delta);
    }

    m_gameState.gameTime += delta;

    m_rootSynchronousExecutionGroup = nullptr;

    TaskBatch* firstTaskBatch = nullptr;
    TaskBatch* lastTaskBatch = nullptr;

    // Prepare task dependencies
    for (SizeType index = 0; index < m_systemExecutionGroups.Size(); index++)
    {
        SystemExecutionGroup& systemExecutionGroup = m_systemExecutionGroups[index];

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

        if (systemExecutionGroup.RequiresGameThread())
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

    for (SystemExecutionGroup& systemExecutionGroup : m_systemExecutionGroups)
    {
        if (!systemExecutionGroup.AllowUpdate() || systemExecutionGroup.RequiresGameThread())
        {
            continue;
        }

        systemExecutionGroup.FinishProcessing();
    }

    if (m_rootSynchronousExecutionGroup != nullptr)
    {
        m_rootSynchronousExecutionGroup->FinishProcessing(/* executeBlocking */ true);

        m_rootSynchronousExecutionGroup = nullptr;
    }

    UpdateDirtyMeshEntities();

#if defined(HYP_DEBUG_MODE) && (defined(HYP_SYSTEM_LOG_PERFORMANCE) || defined(HYP_SYSTEMS_LAG_SPIKE_DETECTION))
    for (SystemExecutionGroup& systemExecutionGroup : m_systemExecutionGroups)
    {
        const PerformanceClock& performanceClock = systemExecutionGroup.GetPerformanceClock();
        const double elapsedTimeMs = performanceClock.Elapsed() / 1000.0;

#ifdef HYP_SYSTEMS_LAG_SPIKE_DETECTION
        if (elapsedTimeMs >= SystemExecutionGroupLagSpikeThreshold)
        {
            HYP_LOG(Entity, Warning, "SystemExecutionGroup spike detected: {} ms", elapsedTimeMs);
        }
#endif
#ifdef HYP_SYSTEM_LOG_PERFORMANCE
        for (const auto& it : systemExecutionGroup.GetPerformanceClocks())
        {
            HYP_LOG(Entity, Debug, "\tSystem {} performance: {}", it.first->GetName(), it.second.Elapsed() / 1000.0);
        }
#endif
    }
#endif
}

void World::CollectViews(Array<View*, SceneAllocator>& outViews)
{
    const uint32 slot = RenderApi::GetRingIndex();

    // set buffered Views for current frame index
    m_viewsPerFrame[slot].Resize(m_views.Size());

    for (SizeType i = 0; i < m_views.Size(); i++)
    {
        m_viewsPerFrame[slot][i] = m_views[i].Get();
    }

    if (m_views.Empty() && m_processViews.Empty())
    {
        return;
    }

    SizeType offset = outViews.Size();
    outViews.Resize(offset + m_processViews.Size() + m_views.Size());

    for (SizeType i = 0; i < m_views.Size(); i++)
    {
        AssertDebug(m_views[i] != nullptr);
        AssertDebug(!m_processViews.Contains(m_views[i]));

        outViews[offset + i] = m_views[i].Get();
    }

    offset += m_views.Size();

    for (SizeType i = 0; i < m_processViews.Size(); i++)
    {
        outViews[offset + i] = m_processViews[i];
    }

    // Clear additional Views to process for next frame
    m_processViews.Clear();
}

void World::CollectSubsystems(Array<Subsystem*, SceneAllocator>& outSubsystems)
{
    const SizeType offset = outSubsystems.Size();
    outSubsystems.Resize(offset + m_subsystemsArray.Size());

    for (SizeType i = 0; i < m_subsystemsArray.Size(); i++)
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
    if (IsReady())
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

    if (IsReady())
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

void World::StartSimulating()
{
    HYP_SCOPE;

    if (m_gameState.mode == GameStateMode::SIMULATING)
    {
        return;
    }

    const GameStateMode previousGameStateMode = m_gameState.mode;

    if (previousGameStateMode == GameStateMode::EDITOR)
    {
        m_gameState.gameTime = 0.0f;
        m_gameState.deltaTime = 0.0f;
    }

    m_gameState.mode = GameStateMode::SIMULATING;

    OnGameStateChange(this, previousGameStateMode, GameStateMode::SIMULATING);
}

void World::StopSimulating()
{
    HYP_SCOPE;

    if (m_gameState.mode != GameStateMode::SIMULATING)
    {
        return;
    }

    const GameStateMode previousGameStateMode = m_gameState.mode;

    // @TODO: Non-editor mode (pause)

    m_gameState.gameTime = 0.0f;
    m_gameState.deltaTime = 0.0f;
    m_gameState.mode = GameStateMode::EDITOR;

    OnGameStateChange(this, previousGameStateMode, GameStateMode::EDITOR);
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

    if (IsReady())
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

        if (IsReady())
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

    SafeDelete(std::move(strongScene));

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

    m_views.PushBack(view);

    if (IsReady())
    {
        if (view->m_raytracingView.GetUnsafe() != m_raytracingView)
        {
            if (view->m_raytracingView)
            {
                HYP_LOG(Scene, Warning,
                    "View {} already has a raytracing View set! Was it added to multiple Worlds with raytracing enabled?",
                    view->Id());

                view->m_raytracingView.Reset();
            }

            if (m_raytracingView != nullptr)
            {
                view->m_raytracingView = m_raytracingView->WeakHandleFromThis();
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

    if (IsReady())
    {
        view->m_raytracingView.Reset();

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

    auto it = m_views.FindIf([view](const Handle<View>& other)
        {
            return other.Get() == view;
        });

    if (it != m_views.End())
    {
        Handle<View> strongView = std::move(*it);

        m_views.Erase(it);

        SafeDelete(std::move(strongView));
    }
}

Span<View* const> World::GetViews() const
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread | g_gameThread);

    return m_viewsPerFrame[RenderApi::GetRingIndex()].ToSpan();
}

void World::UpdateDirtyMeshEntities()
{
    HYP_SCOPE;

    using UpdatedEntitySet = HashSet<Entity*, &KeyBy_Identity<Entity*>, NodeAllocator<SceneAllocator>>;

    UpdatedEntitySet updatedEntities;

    for (const Handle<Scene>& scene : m_scenes)
    {
        if (!scene)
        {
            continue;
        }

        EntityManager* entityManager = scene->GetEntityManager();
        AssertDebug(entityManager != nullptr);

        for (auto [entity, meshComponent, transformComponent, boundingBoxComponent, _] : entityManager->GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, TagComponent<EntityTag::UPDATE_RENDER_PROXY>>())
        {
            HYP_NAMED_SCOPE_FMT("Update draw data for Entity: {}", entity->GetName());

            if (!meshComponent.mesh || !meshComponent.material)
            {
                HYP_LOG_ONCE(Entity, Warning, "Mesh or material not valid for Entity: {}", entity->GetName());

                updatedEntities.Insert(entity);

                continue;
            }

            entity->SetNeedsRenderProxyUpdate();

            if (meshComponent.previousModelMatrix == transformComponent.transform.GetMatrix())
            {
                updatedEntities.Insert(entity);
            }
            else
            {
                meshComponent.previousModelMatrix = transformComponent.transform.GetMatrix();
            }
        }

        if (updatedEntities.Any())
        {
            for (Entity* entity : updatedEntities)
            {
                entityManager->RemoveTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
            }

            updatedEntities.Clear();
        }
    }
}

void World::DeserializeNonStreamingScenes(const Array<Handle<Scene>>& scenes)
{
    HYP_SCOPE;
    // no thread assertion if not yet init since this is used for deserialization mainly

    const bool isReady = IsReady();

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

        if (isReady)
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

        SafeDelete(std::move(scene));
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

        if (isReady)
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
            AssertOnThread(g_gameThread);
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
            AssertOnThread(g_gameThread);
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

    Handle<WorldGridLayer> layer = CreateObject<WorldGridLayer>(streamingLayerName);
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

            // @TODO remove Scenes if layer is scene streaming layer?
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

        m_worldGrid = CreateObject<WorldGrid>(this);
    }

    m_worldGrid->SetStreamingLayersFromDescs(streamingLayers.ToSpan());

    for (const Handle<WorldGridLayer>& layer : m_worldGrid->GetLayers())
    {
        BindStreamingDelegates(m_delegateHandlers, this, layer);

        // @TODO if scene streaming is enabled and we're Init()'d, stream them in!
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

SystemBase* World::AddSystem(const Handle<SystemBase>& system)
{
    Assert(system.IsValid());
    Assert(system->m_world == nullptr || system->m_world == this);

    system->InitComponentInfos_Internal();

    bool wasAdded = false;

    if (system->AllowParallelExecution())
    {
        for (SystemExecutionGroup& systemExecutionGroup : m_systemExecutionGroups)
        {
            if (systemExecutionGroup.IsValidForSystem(system.Get()))
            {
                if (systemExecutionGroup.AddSystem(system))
                {
                    wasAdded = true;

                    break;
                }
            }
        }
    }

    if (!wasAdded)
    {
        SystemExecutionGroup& systemExecutionGroup = m_systemExecutionGroups.EmplaceBack(system->RequiresGameThread(), system->AllowUpdate());

        if (systemExecutionGroup.AddSystem(system))
        {
            wasAdded = true;
        }
    }

    system->m_world = this;

    // If the World is initialized, call Initialize() on the System.
    if (IsInitCalled() && wasAdded)
    {
        InitObject(system);

        for (const Handle<Scene>& scene : m_scenes)
        {
            if (scene != nullptr)
            {
                scene->GetEntityManager()->NotifySystemOfExistingEntities(system);
            }
        }
    }

    return system;
}

} // namespace hyperion
