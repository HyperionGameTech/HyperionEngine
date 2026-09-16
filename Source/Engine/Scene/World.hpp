/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FlatMap.hpp>

#include <Core/Memory/Allocator/ArenaAllocator.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Math/Color.hpp>

#include <Core/Threading/AtomicVar.hpp>

#include <Scripting/ScriptableDelegate.hpp>

#include <Scene/Layer.hpp>
#include <Scene/EnvironmentSettings.hpp>

#include <Framework/EngineMemory.hpp>

namespace Hyperion {

class Game;
class View;
class Scene;
class Camera;
class Subsystem;
class WorldGrid;
class WorldGridLayer;
class PhysicsWorldBase;
class SystemBase;
class Swatch;
class SystemExecutionGroup;

struct GameState;
struct WorldShaderData;

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

struct WGLayerDesc;

class Swatch;
enum class SwatchId : uint32;

// clang-format off

HYP_ENUM()
enum class WorldFlags : uint32
{
    None = 0x0,                                                     //!< @editor=false

    Editor = 0x1,                                                   //!< @editor=false @title="Editor World (internal)" @description="If set, the World is an editor world. (single world created for the editor environment itself)"

    HasPhysics = 0x2,                                               //!< @title="Enable Physics" @description="If set, the World has a PhysicsWorld associated with it and will perform physics simulation."
    HasStreaming = 0x4,                                             //!< @title="Enable Streaming" @description="If set, the World has a grid for spatial partitioning and streaming."
    IsReplicated = 0x8,                                             //!< @title="Is Replicated" @description="If set, the World will have its state replicated over network"

    HasSceneStreamingLayer = 0x100,                                 //!< @title="Scene Streaming" @description="If set, the World has a streaming layer for loading/unloading Scenes based on the WorldGrid."
    AllStreamingLayerFlags = HasSceneStreamingLayer,                //!< @editor=false

    Default = HasPhysics | HasStreaming | IsReplicated | AllStreamingLayerFlags //!< @editor=false
};

// clang-format on

HYP_MAKE_ENUM_FLAGS(WorldFlags);

HYP_CLASS(AssetBucket = "Worlds")
class ENGINE_API World final : public AssetObject
{
    HYP_OBJECT_BODY(World);

    friend class EngineDriver;

public:
    using SubsystemsMap = FlatMap<TypeId, Handle<Subsystem>, SceneAllocator>;

    World();
    explicit World(Name name, EnumFlags<WorldFlags> worldFlags = WorldFlags::Default);

    World(const World& other) = delete;
    World& operator=(const World& other) = delete;

    World(World&& other) noexcept = delete;
    World& operator=(World&& other) noexcept = delete;

    ~World() override;

    void Initialize();
    void Shutdown();

    HYP_METHOD()
    HYP_FORCE_INLINE Game* GetGame() const
    {
        return m_gameInstance;
    }

    /*! \internal Set the game instance for this world */
    HYP_FORCE_INLINE void SetGame(Game* gameInstance)
    {
        m_gameInstance = gameInstance;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<PhysicsWorldBase>& GetPhysicsWorld() const
    {
        return m_physicsWorld;
    }

    HYP_METHOD(Property = "WorldFlags", Serialize)
    HYP_FORCE_INLINE EnumFlags<WorldFlags> GetWorldFlags() const
    {
        return m_worldFlags;
    }

    HYP_METHOD(Property = "WorldFlags", Serialize)
    void SetWorldFlags(EnumFlags<WorldFlags> flags);

    /*! \brief True if this World replicates to clients, either because the process was launched with --server
     *  or because it was flagged with SetIsServerWorld() (e.g. editor Play As Dedicated Server). */
    HYP_METHOD()
    bool IsServerWorld() const;

    /*! \brief Must be called before Initialize(), which is when replication systems are chosen. Not serialized. */
    void SetIsServerWorld(bool isServerWorld);

    HYP_METHOD()
    const Handle<WorldGrid>& GetWorldGrid() const
    {
        return m_worldGrid;
    }

    HYP_METHOD()
    const GameState& GetGameState() const;

    ///Environment Settings

    /// Get EnvironmentSettings - only script bindings for Strata, C# does not have this struct
    HYP_METHOD(Property = "EnvironmentSettings", Serialize, Editor, OnlyLanguages = "strata")
    HYP_FORCE_INLINE const EnvironmentSettings& GetEnvironmentSettings() const
    {
        return m_environmentSettings;
    }

    HYP_METHOD(Property = "EnvironmentSettings", OnlyLanguages = "Strata")
    void SetEnvironmentSettings(const EnvironmentSettings& environmentSettings);

    ///Subsystems

    template <class T>
    HYP_FORCE_INLINE const Handle<T>& AddSubsystem()
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return StaticCast<T>(AddSubsystem(TypeId::ForType<T>(), MakeHandle<T>()));
    }

    template <class T>
    HYP_FORCE_INLINE const Handle<T>& AddSubsystem(const Handle<T>& subsystem)
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return StaticCast<T>(AddSubsystem(TypeId::ForType<T>(), subsystem));
    }

    const Handle<Subsystem>& AddSubsystem(TypeId typeId, const Handle<Subsystem>& subsystem);
    const Handle<Subsystem>& AddSubsystem(const Class* subsystemClass);

    HYP_METHOD()
    bool TryAddSubsystem(const Handle<Subsystem>& subsystem);

    template <class T>
    HYP_FORCE_INLINE T* GetSubsystem()
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return StaticCast<T>(GetSubsystem(TypeId::ForType<T>()));
    }

    Subsystem* GetSubsystem(TypeId typeId) const;

    HYP_METHOD()
    Subsystem* GetSubsystemByName(StringHash name) const;

    HYP_METHOD()
    bool RemoveSubsystem(Subsystem* subsystem);

    ///Swatches

    HYP_METHOD()
    Array<Name> GetSwatchNames() const;

    HYP_METHOD()
    Name GetActiveSwatchName() const;

    HYP_METHOD()
    void SetActiveSwatch(Name swatchName);

    const Handle<Swatch>& GetActiveSwatch();
    const Handle<Swatch>& GetDefaultSwatch();

    const Handle<Swatch>& TryGetSwatch(Name swatchName);
    const Handle<Swatch>& TryGetSwatchById(SwatchId swatchId) const;
    const Handle<Swatch>& GetOrCreateSwatch(Name swatchName);

    ///Layers

    HYP_METHOD()
    HYP_FORCE_INLINE const LayersMask& GetActiveLayers() const
    {
        return m_activeLayers;
    }

    HYP_METHOD()
    Array<Name> GetLayerNames() const;

    const Handle<Layer>& TryGetLayer(Name layerName) const;

    HYP_METHOD()
    const Handle<Layer>& TryGetLayerById(LayerId layerId) const;

    HYP_METHOD()
    const Handle<Layer>& GetOrCreateLayer(Name layerName);

    HYP_METHOD()
    bool IsLayerActive(Name layerName) const;

    HYP_METHOD()
    void SetLayerActive(Name layerName, bool layerActive);

    ///Scenes

    HYP_METHOD()
    void AddScene(const Handle<Scene>& scene, bool addToStreamingLayer = true);

    HYP_METHOD()
    bool RemoveScene(Scene* scene, bool removeFromStreamingLayer = true);

    /*! \brief Get the number of Scenes in the World. Must be called on the sim thread.
     *  \return The number of Scenes in the World. */
    HYP_METHOD()
    bool HasScene(ObjId<Scene> sceneId) const;

    /*! \brief Find a Scene by its Name property. If no Scene with the given name exists, an empty handle is returned. Must be called on the sim thread.
     *  \param nameHash The hashed name of the Scene to find.
     *  \return The Scene with the given name, or an empty handle if no Scene with the given name exists. */
    HYP_METHOD()
    const Handle<Scene>& GetSceneByName(StringHash nameHash) const;

    HYP_METHOD()
    const Array<Handle<Scene>>& GetScenes() const
    {
        return m_scenes;
    }

    ///View

    HYP_METHOD()
    void AddView(View* view);

    HYP_METHOD()
    void RemoveView(View* view);

    /*! \brief Get Views attached to this World. Buffered so it is safe to access from either the render thread or sim thread. */
    Span<View* const> GetViews() const;

    ///sim thread only - the live list AddView() / RemoveView() maintain, never holding a removed View
    Span<View* const> GetSimThreadViews() const;

    /*! \brief Copy this frame's Views into render thread owned storage */
    void SnapshotViewsForRender();

    /*! \brief Adds a View for processing asynchronously for this frame. */
    void ProcessViewAsync(View* view);

    /*! \brief Matches the Scenes of a View with the ALL_FOREGROUND_SCENES flag to this World's foreground Scenes.
     *  Views added with AddView() are kept in sync already; this is for Views that are only processed via ProcessViewAsync(). Sim thread only. */
    void SyncViewForegroundScenes(View* view) const;

    ///Systems

    /*! \brief Adds a System to the World.
     *  \param[in] system The System to add.
     *  \returns If the system was successfully added, a pointer to the System instance.
     *   If a System of the exact same type already exists, a pointer to that system will be returned.
     *   Otherwise, nullptr will be returned.
     */
    HYP_METHOD()
    SystemBase* AddSystem(const Handle<SystemBase>& system);

    /* \brief Adds a System of the given type to the World.
     *  \tparam SystemType The type of System to add.
     *  \tparam Args The types of the arguments to pass to the System's constructor.
     *  \param[in] args The arguments to pass to the System's constructor.
     *  \returns If the system was successfully added, a pointer to the System instance.
     *   If a System of the exact same type already exists, a pointer to that system will be returned.
     *   Otherwise, nullptr will be returned.
     */
    template <class SystemType, class... Args>
    SystemBase* AddSystemT(Args&&... args)
    {
        SystemType* existingSystem = GetSystem<SystemType>();
        if (existingSystem != nullptr)
        {
            return existingSystem;
        }

        return AddSystem(MakeHandle<SystemType>(std::forward<Args>(args)...));
    }

    /*! \brief Remove a system from this world.
     *  \param[in] system The system to remove from this world.
     *  \returns A boolean indicating if the system was successfully removed or not. */
    HYP_METHOD()
    bool RemoveSystem(SystemBase* system);

    /*! \brief Get a system instance by type. If a system with the exact type exists, that will be returned.
     *  A second pass using IsA() to check for type matches is performed if the first one doesn't succeed,
     *  allowing for SystemType to be a base class of the instance's class.
     *  \tparam SystemType The type to query for
     *  \returns A pointer to the system if one exists on this world, nullptr otherwise. */
    template <class SystemType>
    SystemType* GetSystem() const
    {
        auto it = m_systems.FindIf([staticClass = SystemType::StaticClass()](const Handle<SystemBase>& system)
                                   {
                                       return reinterpret_cast<const ObjectBase*>(system.Get())->InstanceClass() == staticClass;
                                   });

        if (it != m_systems.End())
        {
            return DynamicCast<SystemType>(*it);
        }

        it = m_systems.FindIf([](const Handle<SystemBase>& system)
                              {
                                  return Hyperion::IsA(SystemType::StaticClass(), reinterpret_cast<const ObjectBase*>(system.Get())->InstanceClass());
                              });

        if (it != m_systems.End())
        {
            return DynamicCast<SystemType>(*it);
        }

        return nullptr;
    }

    template <class SystemType>
    HYP_FORCE_INLINE bool HasSystem() const
    {
        return GetSystem<SystemType>() != nullptr;
    }

    ////////////////////

    HYP_FORCE_INLINE const Array<SystemExecutionGroup*>& GetSystemExecutionGroups() const
    {
        return m_systemExecutionGroups;
    }

    /*! \brief Gets the View responsible for collecting objects used in ray tracing. Will return nullptr if ray tracing is not enabled. */
    HYP_FORCE_INLINE View* GetRayTracingView() const
    {
        return m_rayTracingView;
    }

    void CollectScenes(Array<Scene*, SceneTempAllocator>& outScenes);
    void CollectCameras(Array<Camera*, SceneTempAllocator>& outCameras);
    void CollectViews(Array<View*, SceneTempAllocator>& outViews);
    void CollectSubsystems(Array<Subsystem*, SceneTempAllocator>& outSubsystems);

    void BeginUpdate(TaskBatch& inBatch, float delta);
    void EndUpdate();

    void FillWorldShaderData(WorldShaderData& outShaderData) const;

    HYP_FIELD()
    static ScriptableDelegate<void, World*, const Handle<Scene>& /* scene */> OnSceneAdded;

    HYP_FIELD()
    static ScriptableDelegate<void, World*, Scene* /* scene */> OnSceneRemoved;

    HYP_FIELD()
    static ScriptableDelegate<void, Name> OnActiveSwatchChanged;

    HYP_FIELD()
    static ScriptableDelegate<void, LayersMask> OnActiveLayersChanged;

    HYP_FIELD()
    static ScriptableDelegate<void, const EnvironmentSettings&> OnEnvironmentSettingsChanged;

private:
    void SyncPhysicsToEntities();
    void SyncPhysicsBodyKinematicStates();

    bool AddSystemToExecutionGroup(SystemBase* system);

    void UpdateReplicationSystems();

    Handle<WorldGridLayer> GetOrCreateStreamingLayer(Name streamingLayerName);
    Handle<WorldGridLayer> GetStreamingLayer(Name streamingLayerName) const;

    ///Serialization Only Properties --

    /// Needs Swatches to load before
    HYP_METHOD(Property = "NonStreamingScenes", Serialize, LoadOrder = 5)
    void DeserializeNonStreamingScenes(const Array<Handle<Scene>>& scenes);

    HYP_METHOD(Property = "NonStreamingScenes")
    Array<Handle<Scene>> SerializeNonStreamingScenes() const;

    HYP_METHOD(Property = "StreamingLayers", Serialize, LoadOrder = 100)
    void DeserializeStreamingLayers(const Array<WGLayerDesc, DynamicAllocator>& streamingLayers);

    HYP_METHOD(Property = "StreamingLayers")
    Array<WGLayerDesc, DynamicAllocator> SerializeStreamingLayers() const;

    HYP_METHOD(Property = "Systems", Serialize)
    void DeserializeSystems(const Array<Handle<SystemBase>>& systems);

    HYP_METHOD(Property = "Systems", Serialize)
    Array<Handle<SystemBase>> SerializeSystems() const;

    // Must load before we load the actual Layers!
    HYP_METHOD(Property = "ActiveLayers", Serialize, LoadOrder = 1)
    void DeserializeActiveLayers(const Array<Name>& activeLayers);

    HYP_METHOD(Property = "ActiveLayers", Serialize)
    Array<Name> SerializeActiveLayers() const;

    ////////////////////

    HYP_FIELD(Property = "GameInstance", Transient)
    Game* m_gameInstance;

    HYP_FIELD(Property = "WorldFlags", Serialize, LoadOrder = 0)
    EnumFlags<WorldFlags> m_worldFlags;

    HYP_FIELD(Property = "Environment")
    EnvironmentSettings m_environmentSettings;

    HYP_FIELD(Property = "Scenes", Transient)
    Array<Handle<Scene>> m_scenes;

    ///Swatches

    HYP_FIELD(Property = "Swatches", Serialize, LoadOrder = 0)
    Array<Handle<Swatch>> m_swatches;

    HYP_FIELD(Property = "ActiveSwatch", Serialize, LoadOrder = 0)
    Name m_activeSwatch;
    
    HYP_FIELD(Property = "ActiveSwatchId", Transient)
    SwatchId m_activeSwatchId;

    ///Layers

    HYP_FIELD(Property = "Layers", Serialize, LoadOrder = 0)
    Array<Handle<Layer>> m_layers;

    HYP_FIELD(Property = "ActiveLayers", Serialize)
    LayersMask m_activeLayers;

    ///Systems

    // systems must load after flags are set
    HYP_FIELD(Property = "Systems", LoadOrder = 200)
    Array<Handle<SystemBase>> m_systems;

    Array<SystemExecutionGroup*> m_systemExecutionGroups;
    SystemExecutionGroup* m_rootSynchronousExecutionGroup;

    ///Views

    /// Sim thread owned views list
    Array<View*, SceneAllocator> m_views;

    /// Views, buffered so the render thread can safely read from it
    Array<View*> m_viewsPerFrame[RingBufferDepth];

    /// Render thread's own copy, taken during the exclusive window
    Array<View*> m_viewsRenderSnapshot;

    View* m_rayTracingView;

    ////////////////////

    SubsystemsMap m_subsystems;
    Array<Subsystem*, SceneAllocator> m_subsystemsArray;

    Handle<WorldGrid> m_worldGrid;

    Handle<PhysicsWorldBase> m_physicsWorld;

    // additional views to process for the current frame
    Array<View*, SceneAllocator> m_processViews;

    DelegateHandlerSet m_delegateHandlers;

    bool m_isInitialized;
    bool m_isServerWorld;
};

} // namespace Hyperion
