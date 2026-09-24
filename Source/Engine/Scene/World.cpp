/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityTag.hpp>
#include <Scene/SystemExecutionGroup.hpp>
#include <Scene/Swatch.hpp>
#include <Scene/Subsystem.hpp>
#include <Scene/Light/Light.hpp>
#include <Scene/LOD.hpp>

#include <Scene/Sky/DynamicSkySystem.hpp>

#include <Scene/Systems/VisibilityStateUpdaterSystem.hpp>
#include <Scene/Systems/LightmapSystem.hpp>
#include <Scene/Systems/AnimationSystem.hpp>
#include <Scene/Systems/AudioSystem.hpp>
#include <Scene/Systems/PhysicsSystem.hpp>
#include <Scene/Systems/CameraSystem.hpp>
#include <Scene/Systems/CharacterControllerSystem.hpp>
#include <Scene/Systems/PlayerSystem.hpp>
#include <Scene/Systems/ScriptSystem.hpp>
#include <Scene/Systems/MeshSystem.hpp>
#include <Scene/Systems/TerrainLodSystem.hpp>
#include <Scene/Systems/ReplicationSystem.hpp>
#include <Scene/Systems/ReplicationApplySystem.hpp>
#include <Scene/Systems/SwatchOverrideSystem.hpp>
#include <Scene/Systems/WeaponSystem.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/TerrainCellComponent.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Scripting/EntityScripting.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Task.hpp>
#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/DataRaceDetector.hpp>

#include <Core/Utilities/BitField.hpp>
#include <Core/Functional/Proc.hpp>

#include <Core/Config/Config.hpp>

#include <System/AppContext.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxy.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Framework/Game.hpp>
#include <Framework/EngineDriver.hpp>
#include <Framework/EngineStats.hpp>
#include <Framework/CVarManager.hpp>
#include <Framework/View.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Physics/PhysicsWorld.hpp>

#include <World.generated.inl>

namespace Hyperion {

ScriptableDelegate<void, World*, const Handle<Scene>&> World::OnSceneAdded;
ScriptableDelegate<void, World*, Scene*> World::OnSceneRemoved;
ScriptableDelegate<void, Name> World::OnActiveSwatchChanged;
ScriptableDelegate<void, LayersMask> World::OnActiveLayersChanged;
ScriptableDelegate<void, const EnvironmentSettings&> World::OnEnvironmentSettingsChanged;

#define HYP_WORLD_ASYNC_SUBSYSTEM_UPDATES
#define HYP_WORLD_ASYNC_VIEW_COLLECTION

// #define HYP_SYSTEMS_LAG_SPIKE_DETECTION
// #define HYP_SYSTEM_LOG_PERFORMANCE

// if the number of systems in a group is less than this value, they will be executed sequentially
static constexpr double SystemExecutionGroupLagSpikeThreshold = 50.0;

extern CVar<bool> g_cvRayTracingEnabled;

EngineStatTimer g_statScriptUpdate("Script/Update");
static EngineStatTimer s_statPhysicsUpdate("Physics/Update");

static const Name s_nameStreamingLayerScenes = NAME("Scenes_Layer");
static const Name s_nameUnnamedWorld = NAME("<unnamed world>");

World::World()
    : World(s_nameUnnamedWorld)
{
}

World::World(Name name, EnumFlags<WorldFlags> worldFlags)
    : AssetObject(name),
      m_gameInstance(nullptr),
      m_worldFlags(worldFlags),
      m_rootSynchronousExecutionGroup(nullptr),
      m_rayTracingView(nullptr),
      m_activeSwatchId(InvalidSwatchId),
      m_isInitialized(false),
      m_isServerWorld(false),
      m_activeLayers {}
{
    if (m_worldFlags & WorldFlags::AllStreamingLayerFlags)
    {
        Assert(m_worldFlags & WorldFlags::HasStreaming, "Streaming layers require streaming to be enabled!");
    }
}

World::~World()
{
    AssertDebug(!m_isInitialized, "World {} is still in init state in dtor", m_name);

    Shutdown();

    OnSceneAdded.RemoveAllForTarget(this);
    OnSceneRemoved.RemoveAllForTarget(this);
    OnActiveSwatchChanged.RemoveAllForTarget(this);
    OnActiveLayersChanged.RemoveAllForTarget(this);
    OnEnvironmentSettingsChanged.RemoveAllForTarget(this);
}

void World::Initialize()
{
    if (m_isInitialized)
    {
        return;
    }

    if (m_worldFlags & WorldFlags::HasStreaming)
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
    if (!EngineGlobals::IsHeadless() && g_cvRayTracingEnabled.Get())
    {
        // dummy output target
        FramebufferDesc framebufferDesc;
        framebufferDesc.extent = Vec2u::One();
        framebufferDesc.attachments[0] = { TextureType::Texture2D, TextureFormat::R8 };
        framebufferDesc.numAttachments = 1;

        Camera* rayTracingViewCamera = new Camera;
        rayTracingViewCamera->SetName(NAME("RayTracingViewDummyCamera"));

        ViewDesc rayTracingViewDesc {};
        rayTracingViewDesc.flags = ViewFlags::RAY_TRACING | ViewFlags::NO_DRAW_CALLS
            | ViewFlags::ALL_FOREGROUND_SCENES | ViewFlags::COLLECT_ALL_ENTITIES
            | ViewFlags::SKIP_LIGHTS
            | ViewFlags::SKIP_LIGHTMAP_VOLUMES
            | ViewFlags::SKIP_ENV_PROBES
            | ViewFlags::SKIP_CAMERAS
            | ViewFlags::SKIP_FOG_VOLUMES
            | ViewFlags::SKIP_PARTICLE_VOLUMES
            | ViewFlags::NO_FRUSTUM_CULLING
            | ViewFlags::NO_ASYNC_SHADER_LOADING;

        rayTracingViewDesc.framebufferDesc = framebufferDesc;
        rayTracingViewDesc.camera = rayTracingViewCamera;

        View* rayTracingView = new View(rayTracingViewDesc);
        rayTracingView->SetName(NAME("RayTracingView"));
        InitObject(rayTracingView);

        m_rayTracingView = rayTracingView;

        m_views.PushBack(rayTracingView);
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        scene->SetWorld(this);

        scene->Initialize();

        OnSceneAdded.Fire(this, this, scene);

        for (Subsystem* subsystem : m_subsystemsArray)
        {
            subsystem->OnSceneAttached(scene);
        }

        if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
        {
            for (View* view : m_views)
            {
                if (!(view->GetFlags() & ViewFlags::ALL_FOREGROUND_SCENES))
                {
                    continue;
                }

                view->AddScene(scene);
            }
        }

        // add to streaming layer if applicable
        if ((m_worldFlags & WorldFlags::HasSceneStreamingLayer) && (scene->GetSceneFlags() & SceneFlags::STREAMED))
        {
            Handle<WorldGridLayer> scenesStreamingLayer = GetOrCreateStreamingLayer(s_nameStreamingLayerScenes);
            AssertDebug(scenesStreamingLayer != nullptr);
            scenesStreamingLayer->AddStreamingObject(scene, scene->GetStreamingCentroid());
        }
    }

    if (m_worldFlags & WorldFlags::HasPhysics)
    {
        if (!m_physicsWorld)
        {
            m_physicsWorld = MakeHandle<PhysicsWorld>();
        }

        m_physicsWorld->Initialize();

        if (!HasSystem<PhysicsSystem>())
        {
            AddSystem(MakeHandle<PhysicsSystem>());
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

    if (!HasSystem<ScriptSystem>())
        AddSystem(MakeHandle<ScriptSystem>());

    if (!HasSystem<MeshSystem>())
        AddSystem(MakeHandle<MeshSystem>());

    if (!HasSystem<TerrainLodSystem>())
        AddSystem(MakeHandle<TerrainLodSystem>());

    if (!HasSystem<CameraSystem>())
        AddSystem(MakeHandle<CameraSystem>());

    if (!HasSystem<SwatchOverrideSystem>())
        AddSystem(MakeHandle<SwatchOverrideSystem>());
    
    if (!HasSystem<WeaponSystem>())
        AddSystem(MakeHandle<WeaponSystem>());

    if (!(m_worldFlags & WorldFlags::Editor))
    {
        if (!HasSystem<CharacterControllerSystem>())
            AddSystem(MakeHandle<CharacterControllerSystem>());

        if (!HasSystem<PlayerSystem>())
            AddSystem(MakeHandle<PlayerSystem>());
    }

    UpdateReplicationSystems();

    if (m_activeSwatchId == Invalid<SwatchId>)
    {
        if (!m_activeSwatch)
        {
            // Set to default swatch if no ActiveSwatch
            m_activeSwatch = g_defaultSwatchName;
        }

        const Handle<Swatch>& swatch = GetOrCreateSwatch(m_activeSwatch);
        Assert(swatch.IsValid());

        m_activeSwatchId = swatch->swatchId;

        OnActiveSwatchChanged.Fire(this, m_activeSwatch);
    }

    m_isInitialized = true;

    for (SystemBase* system : m_systems)
    {
        system->InitComponentInfos_Internal();

        const bool wasAddedToExecutionGroup = AddSystemToExecutionGroup(system);
        Assert(wasAddedToExecutionGroup);

        system->m_world = this;

        system->OnAddedToWorld(this);

        for (const Handle<Scene>& scene : m_scenes)
        {
            scene->GetEntityManager()->NotifySystemOfExistingEntities(system);
        }
    }

    for (View* view : m_views)
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
}

void World::Shutdown()
{
    if (!m_isInitialized)
    {
        return;
    }

    m_isInitialized = false;

    m_processingScenes.Clear();

    Array<Handle<Scene>> scenes = std::move(m_scenes);

    for (const Handle<Scene>& scene : scenes)
    {
        if (!scene)
        {
            continue;
        }

        scene->SetOwnerThreadId(CurrentThreadId());

        OnSceneRemoved.Fire(this, this, scene);

        for (Subsystem* subsystem : m_subsystemsArray)
        {
            subsystem->OnSceneDetached(scene);
        }

        if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
        {
            for (View* view : m_views)
            {
                if (!(view->GetFlags() & ViewFlags::ALL_FOREGROUND_SCENES))
                {
                    continue;
                }

                view->RemoveScene(scene);
            }
        }

        scene->Shutdown();
    }

    scenes.Clear();

    for (const Handle<SystemBase>& system : m_systems)
    {
        system->OnRemovedFromWorld(this);
        system->m_world = nullptr;
    }

    m_systems.Clear();

    for (SystemExecutionGroup* executionGroup : m_systemExecutionGroups)
    {
        PoolDelete(*g_scenePool, executionGroup);
    }
    m_systemExecutionGroups.Clear();

    if (m_rayTracingView != nullptr)
    {
        Camera* rayTracingViewCamera = m_rayTracingView->GetCamera();
        AssertDebug(rayTracingViewCamera != nullptr);

        m_rayTracingView->SetCamera(nullptr);

        rayTracingViewCamera->Release();

        m_rayTracingView = nullptr;
    }

    for (View* view : m_views)
    {
        view->Release();
    }

    m_views.Clear();

    for (Subsystem* subsystem : m_subsystemsArray)
    {
        subsystem->OnRemovedFromWorld();
    }

    m_subsystemsArray.Clear();
    m_subsystems.Clear();

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

void World::SetWorldFlags(EnumFlags<WorldFlags> flags)
{
    uint32 changedFlags = uint32(m_worldFlags) ^ uint32(flags);

    if (changedFlags == 0)
    {
        return;
    }

    m_worldFlags = flags;

    if (changedFlags & uint32(WorldFlags::HasPhysics))
    {
        if (m_worldFlags & WorldFlags::HasPhysics)
        {
            if (!m_physicsWorld)
            {
                m_physicsWorld = MakeHandle<PhysicsWorld>();
            }

            m_physicsWorld->Initialize();

            if (!HasSystem<PhysicsSystem>())
            {
                AddSystem(MakeHandle<PhysicsSystem>());
            }
        }
        else
        {
            if (m_physicsWorld)
            {
                m_physicsWorld->Teardown();
                m_physicsWorld.Reset();
            }

            PhysicsSystem* physicsSystem = GetSystem<PhysicsSystem>();

            if (physicsSystem != nullptr)
            {
                RemoveSystem(physicsSystem);
            }
        }
    }

    // Same thing for replication
    if (changedFlags & uint32(WorldFlags::IsReplicated))
    {
        UpdateReplicationSystems();
    }

    bool needToShutdownWorldGrid = false;

    if (changedFlags & uint32(WorldFlags::HasStreaming))
    {
        if (m_worldFlags & WorldFlags::HasStreaming)
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
                const EnumFlags<WorldFlags> streamingLayerFlagsBefore = m_worldFlags & WorldFlags::AllStreamingLayerFlags;
                m_worldFlags &= ~WorldFlags::AllStreamingLayerFlags;

                // add changed flags for all streaming layers that were on before -  we handle them below
                changedFlags |= uint32(m_worldFlags & WorldFlags::AllStreamingLayerFlags) ^ uint32(streamingLayerFlagsBefore);
            }
        }
    }

    if (changedFlags & uint32(WorldFlags::HasSceneStreamingLayer))
    {
        if (m_worldFlags & WorldFlags::HasSceneStreamingLayer)
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

void World::SetEnvironmentSettings(const EnvironmentSettings& environmentSettings)
{
    if (memcmp(&m_environmentSettings, &environmentSettings, sizeof(EnvironmentSettings)) == 0)
    {
        return;
    }

    m_environmentSettings = environmentSettings;
    
    OnEnvironmentSettingsChanged.Fire(this, m_environmentSettings);
}

#pragma region Swatches

const Handle<Swatch>& World::GetOrCreateSwatch(Name swatchName)
{
    auto it = m_swatches.FindIf([&swatchName](const Handle<Swatch>& swatch)
    {
        if (!swatch)
        {
            return false;
        }

        return swatch->name == swatchName;
    });

    if (it != m_swatches.End())
    {
        return *it;
    }

    BitField<MaxSwatchesPerWorld> usedIds {};

    for (const Handle<Swatch>& swatch : m_swatches)
    {
        Assert(swatch);

        if (!swatch)
        {
            continue;
        }

        if (uint32(swatch->swatchId) < MaxSwatchesPerWorld)
        {
            usedIds.Set(uint32(swatch->swatchId), true);
        }
    }

    uint64 freeId = (~usedIds).FirstOneBit();

    if (freeId >= MaxSwatchesPerWorld)
    {
        HYP_LOG(Scene, Error, "Cannot create Swatch '{}': maximum of {} Swatches already exist on World {}", swatchName, MaxSwatchesPerWorld, GetName());

        return Handle<Swatch>::Null();
    }

    return m_swatches.PushBack(MakeHandle<Swatch>(swatchName, SwatchId(freeId)));
}

Array<Name> World::GetSwatchNames() const
{
    AssertOnThread(g_simThread);

    return MapToArray(m_swatches, [](const Handle<Swatch>& swatch)
        {
            if (!swatch.IsValid())
            {
                return Name();
            }

            return swatch->name;
        });
}

Name World::GetActiveSwatchName() const
{
    AssertOnThread(g_simThread);

    if (!m_activeSwatch)
    {
        return g_defaultSwatchName;
    }

    return m_activeSwatch;
}

void World::SetActiveSwatch(Name swatchName)
{
    AssertOnThread(g_simThread);

    if (swatchName == Name::Invalid())
    {
        swatchName = g_defaultSwatchName;
    }

    const Handle<Swatch>& swatch = GetOrCreateSwatch(swatchName);

    m_activeSwatch = swatchName;
    m_activeSwatchId = swatch->swatchId;

    if (SwatchOverrideSystem* swatchOverrideSystem = GetSystem<SwatchOverrideSystem>())
    {
        swatchOverrideSystem->ApplyActive();
    }

    if (LightmapSystem* lightmapSystem = GetSystem<LightmapSystem>())
    {
        lightmapSystem->ResolveVolumeAssignments();
    }

    OnActiveSwatchChanged.Fire(this, m_activeSwatch);

    // this gets saved on the World asset
    MarkDirty();
}

const Handle<Swatch>& World::GetActiveSwatch()
{
    AssertOnThread(g_simThread);

    Name activeSwatch = m_activeSwatch;

    if (!activeSwatch)
    {
        activeSwatch = g_defaultSwatchName;
    }

    return GetOrCreateSwatch(activeSwatch);
}

const Handle<Swatch>& World::GetDefaultSwatch()
{
    AssertOnThread(g_simThread);

    return GetOrCreateSwatch(g_defaultSwatchName);
}

const Handle<Swatch>& World::TryGetSwatch(Name swatchName)
{
    AssertOnThread(g_simThread);

    auto it = m_swatches.FindIf([&swatchName](const Handle<Swatch>& swatch)
    {
        if (!swatch)
        {
            return false;
        }

        return swatch->name == swatchName;
    });

    if (it == m_swatches.End())
    {
        return Handle<Swatch>::Null();
    }

    return *it;
}

const Handle<Swatch>& World::TryGetSwatchById(SwatchId swatchId) const
{
    AssertOnThread(g_simThread);

    auto it = m_swatches.FindIf([swatchId](const Handle<Swatch>& swatch)
    {
        if (!swatch)
        {
            return false;
        }

        return swatch->swatchId == swatchId;
    });

    if (it == m_swatches.End())
    {
        return Handle<Swatch>::Null();
    }

    return *it;
}

#pragma endregion Swatches

#pragma region Layers

Array<Name> World::GetLayerNames() const
{
    AssertOnThread(g_simThread);

    return MapToArray(m_layers, [](const Handle<Layer>& layer)
        {
            if (!layer.IsValid())
            {
                return Name();
            }

            return layer->name;
        });
}

const Handle<Layer>& World::TryGetLayer(Name layerName) const
{
    AssertOnThread(g_simThread);

    auto it = m_layers.FindIf([&layerName](const Handle<Layer>& layer)
    {
        if (!layer)
        {
            return false;
        }

        return layer->name == layerName;
    });

    if (it == m_layers.End())
    {
        return Handle<Layer>::Null();
    }

    return *it;
}

const Handle<Layer>& World::TryGetLayerById(LayerId layerId) const
{
    AssertOnThread(g_simThread);

    auto it = m_layers.FindIf([layerId](const Handle<Layer>& layer)
    {
        if (!layer)
        {
            return false;
        }

        return layer->layerId == layerId;
    });

    if (it == m_layers.End())
    {
        return Handle<Layer>::Null();
    }

    return *it;
}

const Handle<Layer>& World::GetOrCreateLayer(Name layerName)
{
    AssertOnThread(g_simThread);

    auto it = m_layers.FindIf([&layerName](const Handle<Layer>& layer)
    {
        if (!layer)
        {
            return false;
        }

        return layer->name == layerName;
    });

    if (it != m_layers.End())
    {
        return *it;
    }

    BitField<MaxLayersPerWorld> usedIds {};

    for (const Handle<Layer>& layer : m_layers)
    {
        Assert(layer);

        if (!layer)
        {
            continue;
        }

        if (uint32(layer->layerId) < MaxLayersPerWorld)
        {
            usedIds.Set(uint32(layer->layerId), true);
        }
    }

    uint64 freeId = (~usedIds).FirstOneBit();

    if (freeId >= MaxLayersPerWorld)
    {
        HYP_LOG(Scene, Error, "Cannot create Layer '{}': maximum of {} Layers already exist on World {}", layerName, MaxLayersPerWorld, GetName());

        return Handle<Layer>::Null();
    }

    const LayerId layerId = LayerId(freeId);
    
    m_activeLayers.Set(freeId, true);

    // New layer created, mark the world as dirtied so it gets saved on next save.
    MarkDirty();

    const Handle<Layer>& layer = m_layers.PushBack(MakeHandle<Layer>(layerName, layerId));

    OnActiveLayersChanged.Fire(this, m_activeLayers);

    return layer;
}

bool World::IsLayerActive(Name layerName) const
{
    AssertOnThread(g_simThread);

    const Handle<Layer>& layer = TryGetLayer(layerName);

    if (!layer || uint32(layer->layerId) >= MaxLayersPerWorld)
    {
        return false;
    }

    return m_activeLayers.Test(uint32(layer->layerId));
}

void World::SetLayerActive(Name layerName, bool layerActive)
{
    AssertOnThread(g_simThread);

    const Handle<Layer>& layer = TryGetLayer(layerName);

    if (!layer || uint32(layer->layerId) >= MaxLayersPerWorld)
    {
        HYP_LOG(Scene, Warning, "Cannot change active state of Layer '{}' which does not exist on World {}", layerName, GetName());

        return;
    }

    if (m_activeLayers.Test(uint32(layer->layerId)) == layerActive)
    {
        return;
    }

    m_activeLayers.Set(uint32(layer->layerId), layerActive);

    OnActiveLayersChanged.Fire(this, m_activeLayers);

    // this gets saved on the World asset
    MarkDirty();
}

#pragma endregion Layers

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

void World::SyncViewForegroundScenes(View* view) const
{
    AssertOnThread(g_simThread);

    if (!view || !(view->GetFlags() & ViewFlags::ALL_FOREGROUND_SCENES))
    {
        return;
    }

    const Array<Scene*>& viewScenes = view->GetScenes();

    for (size_t index = viewScenes.Size(); index > 0; index--)
    {
        Scene* viewScene = viewScenes[index - 1];

        // only compares pointers, the Scene may have been destroyed since it was removed from the World
        const bool isInWorld = m_scenes.FindIf([viewScene](const Handle<Scene>& scene)
                                   {
                                       return scene.Get() == viewScene;
                                   })
            != m_scenes.End();

        if (!isInWorld)
        {
            view->RemoveScene(viewScene);
        }
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        if (scene && (scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
        {
            view->AddScene(scene);
        }
    }
}

void World::BeginUpdate(TaskBatch& inBatch, float delta)
{
    HYP_SCOPE;

    // runtime component types registered or redefined since last frame (eg a script assembly loaded or reloaded): migrate and
    // restore their components before scripts run, so scripts see them. No-op otherwise
    for (Scene* scene : m_scenes)
    {
        scene->GetEntityManager()->SyncRuntimeComponentTypes();
    }

    if (GetGameState().IsSimulating())
    {
        if (m_physicsWorld != nullptr)
        {
            ENGINE_STAT_SCOPE(&s_statPhysicsUpdate);

            // This needs to run regardless of authority because clients simulate non-replicated entities locally
            SyncPhysicsBodyKinematicStates();

            m_physicsWorld->Tick(delta);

            // must be called before entity managers are locked.
            SyncPhysicsToEntities();
        }

        ENGINE_STAT_SCOPE(&g_statScriptUpdate);
        EntityScripting::UpdateScriptedEntities(*this, delta);
    }

    for (Scene* scene : m_scenes)
    {
        scene->Update(delta);

        scene->GetEntityManager()->Lock();
    }

    m_rootSynchronousExecutionGroup = nullptr;

    m_processingScenes = m_scenes;

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
        systemExecutionGroup.StartProcessing(delta, m_processingScenes.ToSpan());

        if (currentTaskBatch->executors.Empty())
        {
            // The execution group did not enqueue any tasks.
            // Skip, as we don't want to waste cycles for this one.
            continue;
        }

        if (systemExecutionGroup.RequiresSimThread())
        {
            if (m_rootSynchronousExecutionGroup != nullptr)
            {
                // @TODO Review me: what if we overwrite some previous nextBatch? will it become a dangling task?
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

    m_processingScenes.Clear();

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

extern PhysicsShape* GetDefaultPhysicsShape();

void World::SyncPhysicsToEntities()
{
    PhysicsWorld& physicsWorld = static_cast<PhysicsWorld&>(*m_physicsWorld);

    Array<Entity*, SceneTempAllocator> updatedEntities;

    for (Scene* scene : m_scenes)
    {
        // only sync physics to entities for FOREGROUND scenes.
        if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
        {
            continue;
        }

        for (auto [entity, rigidBodyComponent, _] : scene->GetEntityManager()->GetEntitySet<RigidBodyComponent, TagComponent<EntityTag::UpdatePhysicsShape>>())
        {
            Handle<RigidBody>& rigidBody = rigidBodyComponent.rigidBody;

            if (!rigidBody)
            {
                continue;
            }

            // The body caches a raw pointer to the shape it was created with, so a swap on the component
            // (the inspector assigning a different shape, generated collision replacing it) has to be
            // picked up here, otherwise the rebuild below uses the shape that was just replaced.
            rigidBody->shape = rigidBodyComponent.shape.IsValid()
                ? rigidBodyComponent.shape.Get()
                : GetDefaultPhysicsShape();

            physicsWorld.GetAdapter().OnChangePhysicsShape(rigidBody.Get());

            updatedEntities.PushBack(entity);
        }

        for (auto [entity, rigidBodyComponent, _] : scene->GetEntityManager()->GetEntitySet<RigidBodyComponent, TagComponent<EntityTag::UpdatePhysicsMaterial>>())
        {
            Handle<RigidBody>& rigidBody = rigidBodyComponent.rigidBody;

            if (!rigidBody)
            {
                continue;
            }

            physicsWorld.GetAdapter().OnChangePhysicsMaterial(rigidBody.Get());

            updatedEntities.PushBack(entity);
        }

        for (auto [entity, rigidBodyComponent, transformComponent] : scene->GetEntityManager()->GetEntitySet<RigidBodyComponent, TransformComponent>())
        {
            Handle<RigidBody>& rigidBody = rigidBodyComponent.rigidBody;

            if (!rigidBody)
            {
                continue;
            }

            // Kinematic bodies are driven externally (replicated colliders on a client), so their
            // transform is owned by the replication path, not the simulation.
            if (rigidBody->IsKinematic())
            {
                continue;
            }

            const Transform& rigidBodyTransform = rigidBody->GetTransform();

            entity->SetWorldTranslation(rigidBodyTransform.GetTranslation(), TransformChangeType::Simulation);
            entity->SetWorldRotation(rigidBodyTransform.GetRotation(), TransformChangeType::Simulation);
        }
    }

    for (Entity* entity : updatedEntities)
    {
        entity->RemoveTag<EntityTag::UpdatePhysicsMaterial>();
        entity->RemoveTag<EntityTag::UpdatePhysicsShape>();
    }
}

void World::SyncPhysicsBodyKinematicStates()
{
    const Handle<WorldGrid>& worldGrid = GetWorldGrid();

    for (Scene* scene : m_scenes)
    {
        for (auto [entity, rigidBodyComponent, transformComponent] : scene->GetEntityManager()->GetEntitySet<RigidBodyComponent, TransformComponent>())
        {
            Handle<RigidBody>& rigidBody = rigidBodyComponent.rigidBody;

            if (!rigidBody)
            {
                continue;
            }

            const bool isTerrainCell = entity->HasComponent<TerrainCellComponent>();

            bool shouldBeKinematic = (!SceneHelpers::CanSimulateEntityPhysics(*entity) || isTerrainCell)
                && !rigidBody->IsLocallyPredicted();

            // Hold non-static bodies in place until the ground underneath them has streamed in
            if (!shouldBeKinematic && !isTerrainCell
                && worldGrid.IsValid() && worldGrid->IsCollisionPendingAt(rigidBody->GetTransform().GetTranslation()))
            {
                shouldBeKinematic = true;
            }

            if (rigidBody->IsKinematic() != shouldBeKinematic)
            {
                m_physicsWorld->SetRigidBodyKinematic(rigidBody, shouldBeKinematic);
            }

            m_physicsWorld->SetRigidBodyCharacterGhostCollidable(rigidBody, !shouldBeKinematic);
        }
    }
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
        for (auto [camera] : scene->GetEntityManager()->GetEntitySet<EntityType<Camera>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            outCameras.PushBack(camera);
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
            m_viewsPerFrame[slot][i] = m_views[i];
        }

        const size_t offset = m_views.Size();
        for (size_t i = 0; i < m_processViews.Size(); i++)
        {
            View& view = *m_processViews[i];

            // not in m_views, so AddScene() / RemoveScene() never updated its scenes
            SyncViewForegroundScenes(&view);

            m_viewsPerFrame[slot][offset + i] = &view;
        }
    }

    { // add all views to outViews
        size_t offset = outViews.Size();
        outViews.Resize(offset + m_processViews.Size() + m_views.Size());

        for (size_t i = 0; i < m_views.Size(); i++)
        {
            AssertDebug(m_views[i] != nullptr);
            AssertDebug(!m_processViews.Contains(m_views[i]));

            outViews[offset + i] = m_views[i];
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

void World::CollectLODViewDatas(Array<LODViewData, SceneTempAllocator>& outViewDatas)
{
    AssertOnThread(g_simThread);

    const bool preferEditorViews = GetGameState().IsStopped();

    for (View* view : GetSimThreadViews())
    {
        if (!view || !view->ShouldCollectLODs())
        {
            continue;
        }

        const bool isEditorView = bool(view->GetFlags() & ViewFlags::EDITOR_VIEW);

        if (isEditorView == preferEditorViews)
        {
            outViewDatas.EmplaceBack(*view->GetCamera());
        }
    }

    if (outViewDatas.Any())
    {
        return;
    }

    for (View* view : GetSimThreadViews())
    {
        if (view && view->ShouldCollectLODs())
        {
            outViewDatas.EmplaceBack(*view->GetCamera());
        }
    }

    if (outViewDatas.Any())
    {
        return;
    }

    for (const Handle<Scene>& scene : GetScenes())
    {
        if (!scene)
        {
            continue;
        }

        EntityManager* entityManager = scene->GetEntityManager();

        if (!entityManager)
        {
            continue;
        }

        for (auto [camera] : entityManager->GetEntitySet<EntityType<Camera>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!(camera->GetCameraFlags() & CameraFlags::HasStreamingVolume))
            {
                continue;
            }

            outViewDatas.EmplaceBack(*camera);
        }
    }
}

const Handle<Subsystem>& World::AddSubsystem(TypeId typeId, const Handle<Subsystem>& subsystem)
{
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
    // otherwise, it will be initialized when World::Initialize() is called
    if (m_isInitialized)
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

const Handle<Subsystem>& World::AddSubsystem(const Class* subsystemClass)
{
    Assert(subsystemClass != nullptr);

    BoxedValue boxed;
    if (!subsystemClass->CreateInstance(boxed))
    {
        return Handle<Subsystem>::Null();
    }

    Assert(boxed.Is<Subsystem>());

    Handle<Subsystem>& subsystem = boxed.Get<Handle<Subsystem>>();

    if (!subsystem)
    {
        return Handle<Subsystem>::Null();
    }

    return AddSubsystem(subsystem);
}

bool World::TryAddSubsystem(const Handle<Subsystem>& subsystem)
{
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
    const auto it = m_subsystems.Find(typeId);

    if (it == m_subsystems.End())
    {
        return nullptr;
    }

    return it->second.Get();
}

Subsystem* World::GetSubsystemByName(StringHash name) const
{
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

    if (m_isInitialized)
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

    m_scenes.PushBack(scene);

    if (m_isInitialized)
    {
        scene->Initialize();

        OnSceneAdded.Fire(this, this, scene);

        for (Subsystem* subsystem : m_subsystemsArray)
        {
            subsystem->OnSceneAttached(scene);
        }

        if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
        {
            for (View* view : m_views)
            {
                if (!(view->GetFlags() & ViewFlags::ALL_FOREGROUND_SCENES))
                {
                    continue;
                }

                view->AddScene(scene);
            }
        }

        if (addToStreamingLayer && (m_worldFlags & WorldFlags::HasSceneStreamingLayer) && (scene->GetSceneFlags() & SceneFlags::STREAMED))
        {
            Handle<WorldGridLayer> scenesStreamingLayer = GetOrCreateStreamingLayer(s_nameStreamingLayerScenes);
            AssertDebug(scenesStreamingLayer != nullptr);
            scenesStreamingLayer->AddStreamingObject(scene, scene->GetStreamingCentroid());
        }
    }
}

bool World::RemoveScene(Scene* scene, bool removeFromStreamingLayer)
{
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

        if (m_isInitialized)
        {
            if (removeFromStreamingLayer && (m_worldFlags & WorldFlags::HasSceneStreamingLayer))
            {
                if (Handle<WorldGridLayer> scenesStreamingLayer = GetStreamingLayer(s_nameStreamingLayerScenes); scenesStreamingLayer)
                {
                    scenesStreamingLayer->RemoveStreamingObject(scene);
                }
            }

            OnSceneRemoved.Fire(this, this, scene);

            for (Subsystem* subsystem : m_subsystemsArray)
            {
                subsystem->OnSceneDetached(scene);
            }

            for (View* view : m_views)
            {
                view->RemoveScene(scene);
            }
        }
    }

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

const Handle<Scene>& World::GetSceneByName(StringHash nameHash) const
{
    const auto it = m_scenes.FindIf([nameHash](const Handle<Scene>& scene)
                                    {
                                        return scene->GetName() == nameHash;
                                    });

    return it != m_scenes.End() ? *it : Handle<Scene>::Null();
}

void World::AddView(View* view)
{
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

    view->AddRef();

    // Mark all resources used dirty upon add to ensure proper state
    view->m_markAllAsDirty = true;

    m_views.PushBack(view);

    if (m_isInitialized)
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
        if (view->GetFlags() & ViewFlags::ALL_FOREGROUND_SCENES)
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
    if (!view)
    {
        return;
    }

    if (m_isInitialized)
    {
        view->m_rayTracingView.Reset();

        // Remove all scenes from the view, if the view should collect all world scenes
        if (view->GetFlags() & ViewFlags::ALL_FOREGROUND_SCENES)
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

    m_views.Erase(it);

    view->Release();
}

Span<View* const> World::GetViews() const
{
    AssertOnThread(g_renderThread | g_simThread);

    if (IsOnThread(g_renderThread))
    {
        return m_viewsRenderSnapshot.ToSpan();
    }

    return m_viewsPerFrame[GetRingIndex()].ToSpan();
}

Span<View* const> World::GetSimThreadViews() const
{
    AssertOnThread(g_simThread);

    return Span<View* const>(m_views.Data(), m_views.Size());
}

void World::SnapshotViewsForRender()
{
    AssertOnThread(g_renderThread);

    const Array<View*>& views = m_viewsPerFrame[GetRingIndex()];

    m_viewsRenderSnapshot.Resize(views.Size());
    std::copy(views.Begin(), views.End(), m_viewsRenderSnapshot.Begin());
}

void World::DeserializeNonStreamingScenes(const Array<Handle<Scene>>& scenes)
{
    // no thread assertion if not yet init since this is used for deserialization mainly

    Array<Handle<Scene>> previousScenes = std::move(m_scenes);
    m_scenes.Clear();

    for (Handle<Scene>& scene : previousScenes)
    {
        if (!scene)
        {
            continue;
        }

        if (m_worldFlags & WorldFlags::HasSceneStreamingLayer)
        {
            if (Handle<WorldGridLayer> scenesStreamingLayer = GetStreamingLayer(s_nameStreamingLayerScenes); scenesStreamingLayer)
            {
                scenesStreamingLayer->RemoveStreamingObject(scene);
            }
        }

        scene->SetWorld(nullptr);

        if (m_isInitialized)
        {
            OnSceneRemoved.Fire(this, this, scene);

            for (Subsystem* subsystem : m_subsystemsArray)
            {
                subsystem->OnSceneDetached(scene);
            }

            for (View* view : m_views)
            {
                view->RemoveScene(scene);
            }
        }
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

        if (scene->GetSceneFlags() & SceneFlags::EDITOR)
        {
            HYP_LOG(Scene, Warning, "Dropping editor scene {} from world {} NonStreamingScenes", scene->GetName(), GetName());

            continue;
        }

        scene->SetWorld(this);

        m_scenes.PushBack(scene);

        if (m_isInitialized)
        {
            scene->Initialize();

            OnSceneAdded.Fire(this, this, scene);

            for (Subsystem* subsystem : m_subsystemsArray)
            {
                subsystem->OnSceneAttached(scene);
            }

            if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
            {
                for (View* view : m_views)
                {
                    if (!(view->GetFlags() & ViewFlags::ALL_FOREGROUND_SCENES))
                    {
                        continue;
                    }

                    view->AddScene(scene);
                }
            }
        }
    }
}

Array<Handle<Scene>> World::SerializeNonStreamingScenes() const
{
    Array<Handle<Scene>> scenes;
    scenes.Reserve(m_scenes.Size());

    for (const Handle<Scene>& scene : m_scenes)
    {
        if ((m_worldFlags & WorldFlags::HasSceneStreamingLayer) && (scene->GetSceneFlags() & SceneFlags::STREAMED))
        {
            continue;
        }

        if (scene->GetAssetFlags() & AssetObjectFlags::Transient)
        {
            continue;
        }

        // editor-owned scenes (gizmos, asset previews) belong to the editor session, never to the world asset
        if (scene->GetSceneFlags() & SceneFlags::EDITOR)
        {
            continue;
        }

        scenes.PushBack(scene);
    }

    return scenes;
}

static void BindStreamingDelegates(DelegateHandlerSet& set, World* world, WorldGridLayer* layer)
{
    AssertDebug(world != nullptr && layer != nullptr);

    set.Remove(&layer->OnStreamingObjectsLoaded);
    set.Remove(&layer->OnStreamingObjectsUnloaded);

    set.Add(layer->OnStreamingObjectsLoaded.Bind(
        [world](StreamingCell* cell, Span<const AssetObject*> objs)
        {
            AssertOnThread(g_simThread);

            for (const AssetObject* obj : objs)
            {
                if (obj->IsA(Scene::StaticClass()))
                {
                    const Scene* scene = DynamicCast<Scene>(obj);

                    world->AddScene(MakeStrongRef(scene), /* addToStreamingLayer */ false);

                    continue;
                }
            }
        }));

    set.Add(layer->OnStreamingObjectsUnloaded.Bind(
        [world](StreamingCell* cell, Span<const AssetObject*> objs)
        {
            AssertOnThread(g_simThread);

            for (const AssetObject* obj : objs)
            {
                if (obj->IsA(Scene::StaticClass()))
                {
                    const Scene* scene = DynamicCast<Scene>(obj);

                    world->RemoveScene(const_cast<Scene*>(scene), /* removeFromStreamingLayer */ false);

                    continue;
                }
            }
        }));
}

Handle<WorldGridLayer> World::GetOrCreateStreamingLayer(Name streamingLayerName)
{
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

Handle<WorldGridLayer> World::GetStreamingLayer(Name streamingLayerName) const
{
    AssertDebug(streamingLayerName.IsValid());
    if (!streamingLayerName.IsValid() || !m_worldGrid)
    {
        return Handle<WorldGridLayer>::Null();
    }

    auto it = m_worldGrid->GetLayers().FindIf(
        [streamingLayerName](const Handle<WorldGridLayer>& layer)
        {
            return layer->GetName() == streamingLayerName;
        });

    if (it != m_worldGrid->GetLayers().End())
    {
        return *it;
    }

    return Handle<WorldGridLayer>::Null();
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
        if (!(m_worldFlags & WorldFlags::HasStreaming))
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

    if (m_isInitialized)
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

bool World::IsServerWorld() const
{
    return m_isServerWorld || EngineGlobals::IsServer();
}

void World::SetIsServerWorld(bool isServerWorld)
{
    if (m_isInitialized)
    {
        HYP_LOG(Scene, Error, "SetIsServerWorld() called on World {} after it was initialized; ignoring", m_name);

        return;
    }

    m_isServerWorld = isServerWorld;
}

void World::UpdateReplicationSystems()
{
    const bool isReplicated = bool(m_worldFlags & WorldFlags::IsReplicated);
    const bool isServerWorld = IsServerWorld();

    // Saved worlds serialize whichever replication system the saving process had, so the wrong one can be present
    const auto removeSystem = [this](SystemBase* system)
    {
        if (system == nullptr)
        {
            return;
        }

        if (m_isInitialized)
        {
            RemoveSystem(system);

            return;
        }

        if (auto it = m_systems.Find(system); it != m_systems.End())
        {
            m_systems.Erase(it);
        }
    };

    if (isReplicated && isServerWorld)
    {
        if (!HasSystem<ReplicationSystem>())
        {
            AddSystem(MakeHandle<ReplicationSystem>());
        }
    }
    else
    {
        removeSystem(GetSystem<ReplicationSystem>());
    }

    if (isReplicated && !isServerWorld)
    {
        if (!HasSystem<ReplicationApplySystem>())
        {
            AddSystem(MakeHandle<ReplicationApplySystem>());
        }
    }
    else
    {
        removeSystem(GetSystem<ReplicationApplySystem>());
    }
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
        {
            continue;
        }

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
        {
            continue;
        }

        systemsToSerialize.PushBack(system);
    }

    return systemsToSerialize;
}

void World::DeserializeActiveLayers(const Array<Name>& activeLayers)
{
    // Set layer ids from the names. 
    // Expected that m_layers is populated at this point.

    // Clear bitset first
    m_activeLayers = {};

    for (const Name& layerName : activeLayers)
    {
        auto layersIt = m_layers.FindIf([layerName](const Handle<Layer>& layer)
        {
            if (!layer.IsValid())
            {
                return false;
            }

            return layer->name == layerName;
        });

        if (layersIt == m_layers.End())
        {
            HYP_LOG(Scene, Warning, "Attempted to set active layer with name {} but no layer with that name exists!", layerName);
            continue;
        }

        const Handle<Layer>& layer = *layersIt;
        Assert(layer.IsValid());

        m_activeLayers.Set(uint32(layer->layerId), true);
    }
}

Array<Name> World::SerializeActiveLayers() const
{
    Array<Name> layerNames;
    layerNames.Reserve(m_activeLayers.CountOnes());

    for (uint64 bit : m_activeLayers)
    {
        // Get the layer with the layer id == that bit
        auto layersIt = m_layers.FindIf([bit](const Handle<Layer>& layer)
        {
            if (!layer.IsValid())
            {
                return false;
            }

            return uint32(layer->layerId) == uint32(bit);
        });

        if (layersIt == m_layers.End())
        {
            HYP_LOG(Scene, Warning, "Attempted to set active layer with id {} but no layer with that id exists!", bit);
            continue;
        }

        const Handle<Layer>& layer = *layersIt;
        Assert(layer.IsValid());
        
        layerNames.PushBack(layer->name);
    }

    return layerNames;
}

SystemBase* World::AddSystem(const Handle<SystemBase>& system)
{
    Assert(system.IsValid());
    Assert(system->m_world == nullptr || system->m_world == this);

    auto it = m_systems.FindIf(
        [&system](const Handle<SystemBase>& otherSystem)
        {
            return otherSystem->InstanceClass() == system->InstanceClass();
        });

    if (it != m_systems.End())
    {
        // cannot add system of this type if one already exists!
        HYP_LOG(Scene, Warning, "Attempted to add already existant System type {}", it->GetTypeInfo()->name);

        return *it;
    }

    m_systems.PushBack(system);

    // If the World is initialized, call Initialize() on the System.
    if (m_isInitialized)
    {
        system->InitComponentInfos_Internal();

        const bool wasAddedToExecutionGroup = AddSystemToExecutionGroup(system);
        Assert(wasAddedToExecutionGroup);

        system->m_world = this;

        system->OnAddedToWorld(this);

        for (const Handle<Scene>& scene : m_scenes)
        {
            scene->GetEntityManager()->NotifySystemOfExistingEntities(system);
        }
    }

    MarkDirty();

    return system;
}

bool World::RemoveSystem(SystemBase* system)
{
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

    if (m_isInitialized)
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

    MarkDirty();

    return true;
}

bool World::AddSystemToExecutionGroup(SystemBase* system)
{
    Assert(system != nullptr);

    bool wasAdded = false;

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

    if (!wasAdded)
    {
        SystemExecutionGroup*& systemExecutionGroup = m_systemExecutionGroups.EmplaceBack();
        systemExecutionGroup = HYP_POOL_NEW(g_scenePool, SystemExecutionGroup, system->RequiresSimThread(), system->AllowUpdate());

        if (systemExecutionGroup->AddSystem(system))
        {
            wasAdded = true;
        }
    }

    return wasAdded;
}

void World::FillWorldShaderData(WorldShaderData& outShaderData) const
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const GameState& gameState = GetGameState();

    outShaderData.gameTime = gameState.gameTime;

    // game time only moves while simulating
    const float previousGameTime = gameState.IsSimulating() ? gameState.gameTime - gameState.deltaTime : gameState.gameTime;
    outShaderData.windTimeParams = Vec4f(previousGameTime, 0.0f, 0.0f, 0.0f);

    WriteEnvironmentShaderData(m_environmentSettings, outShaderData);

    bool hasSun = false;

    for (Scene* scene : m_scenes)
    {
        if (hasSun)
        {
            break;
        }

        if (!(scene->GetSceneFlags() & SceneFlags::FOREGROUND))
        {
            continue;
        }

        for (auto [light] : scene->GetEntityManager()->GetEntitySet<EntityType<Light>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (light->GetLightType() != LightType::Directional)
            {
                continue;
            }

            outShaderData.sunDirectionIntensity = Vec4f(light->GetWorldTranslation().Normalized(), light->GetIntensity());
            outShaderData.sunColor = Vec4f(light->GetColor());
            outShaderData.environmentFlags |= uint32(WorldEnvironmentFlags::HasSun);

            hasSun = true;

            break;
        }
    }

    outShaderData.skyLightParams.w = 0.0f;

    if (DynamicSkySystem* skySystem = GetSystem<DynamicSkySystem>())
    {
        if (CloudEffectVolume* cloudEffectVolume = skySystem->GetCloudEffectVolume();
                cloudEffectVolume && cloudEffectVolume->GetSettings().enabled)
        {
            outShaderData.skyLightParams.w = MathUtil::Clamp(cloudEffectVolume->GetSettings().shape.coverage, 0.0f, 1.0f);
        }
    }
}

} // namespace Hyperion
