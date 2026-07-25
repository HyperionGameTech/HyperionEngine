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

#include <Scripting/ScriptableDelegate.hpp>

#include <Core/Math/Color.hpp>

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
class SystemExecutionGroup;

struct GameState;

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

struct WGLayerDesc;

HYP_ENUM()
enum class WorldFlags : uint32
{
    NONE = 0x0,

    EDITOR_WORLD = 0x1, //!< If set, the World is an editor world. (single world created for the editor environment itself)

    HAS_PHYSICS = 0x2,   //!< If set, the World has a PhysicsWorld associated with it and will perform physics simulation.
    HAS_STREAMING = 0x4, //!< If set, the World has a grid for spatial partitioning and streaming.

    HAS_SCENE_STREAMING_LAYER = 0x100, //!< If set, the World has a streaming layer for loading/unloading Scenes based on the WorldGrid.
    ALL_STREAMING_LAYER_FLAGS = HAS_SCENE_STREAMING_LAYER,

    DEFAULT = HAS_PHYSICS | HAS_STREAMING | ALL_STREAMING_LAYER_FLAGS
};

HYP_MAKE_ENUM_FLAGS(WorldFlags);

HYP_STRUCT()
struct FogParams
{
    HYP_STRUCT_BODY(FogParams);

    HYP_FIELD()
    Color color = Color(0xF2F8F7FF);

    HYP_FIELD()
    float startDistance = 250.0f;

    HYP_FIELD()
    float endDistance = 1000.0f;
};

HYP_STRUCT()
struct CSMParams
{
    HYP_STRUCT_BODY(CSMParams);

    HYP_FIELD()
    uint32 numCascades = 4;
};

HYP_STRUCT()
struct CSMState
{
    HYP_STRUCT_BODY(CSMState);

    HYP_FIELD(Transient)
    Vec3f playerCenter;
};

HYP_CLASS(AssetBucket = "Worlds")
class ENGINE_API World final : public AssetObject
{
    HYP_OBJECT_BODY(World);

    friend class Game;
    friend class EngineDriver;

public:
    using SubsystemsMap = FlatMap<TypeId, Handle<Subsystem>, SceneAllocator>;

    World();
    explicit World(Name name, EnumFlags<WorldFlags> worldFlags = WorldFlags::DEFAULT);

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

    HYP_METHOD(Property = "WorldFlags", Serialize)
    HYP_FORCE_INLINE EnumFlags<WorldFlags> GetWorldFlags() const
    {
        return m_worldFlags;
    }

    HYP_METHOD(Property = "WorldFlags", Serialize)
    void SetWorldFlags(EnumFlags<WorldFlags> flags);

    HYP_METHOD(Property = "FogParams")
    const FogParams& GetFogParams() const
    {
        return m_fogParams;
    }

    HYP_METHOD(Property = "FogParams")
    void SetFogParams(const FogParams& fogParams);

    HYP_METHOD(Property = "CSMParams")
    const CSMParams& GetCSMParams() const
    {
        return m_csmParams;
    }

    HYP_METHOD(Property = "CSMParams")
    void SetCSMParams(const CSMParams& csmParams);

    HYP_METHOD(Property = "CSMState", Transient)
    const CSMState& GetCSMState() const
    {
        return m_csmState;
    }

    HYP_METHOD(Property = "CSMState", Transient)
    void SetCSMState(const CSMState& csmState)
    {
        m_csmState = csmState;
    }

    /*! \brief Get the placeholder Scene, used for Entities that are not attached to a Scene.
     *  This version of the function allows the caller to specify the thread the Scene uses for entity management.
     *  If the Scene does not exist for the given thread mask, it will be created.
     *\param threadId The thread the Scene should be associated with.
     * \return The handle for the detached Scene for the given thread.
     */
    const Handle<Scene>& GetDetachedScene(const ThreadId& threadId);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<PhysicsWorldBase>& GetPhysicsWorld()
    {
        return m_physicsWorld;
    }

    template <class T>
    HYP_FORCE_INLINE const Handle<T>& AddSubsystem()
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return DynamicCast<T>(AddSubsystem(TypeId::ForType<T>(), MakeHandle<T>()));
    }

    template <class T>
    HYP_FORCE_INLINE const Handle<T>& AddSubsystem(const Handle<T>& subsystem)
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return DynamicCast<T>(AddSubsystem(TypeId::ForType<T>(), subsystem));
    }

    const Handle<Subsystem>& AddSubsystem(TypeId typeId, const Handle<Subsystem>& subsystem);

    const Handle<Subsystem>& AddSubsystem(const Class* subsystemClass);

    HYP_METHOD()
    bool TryAddSubsystem(const Handle<Subsystem>& subsystem);

    template <class T>
    HYP_FORCE_INLINE T* GetSubsystem()
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return DynamicCast<T>(GetSubsystem(TypeId::ForType<T>()));
    }

    Subsystem* GetSubsystem(TypeId typeId) const;

    HYP_METHOD()
    Subsystem* GetSubsystemByName(StringHash name) const;

    HYP_METHOD()
    bool RemoveSubsystem(Subsystem* subsystem);

    HYP_METHOD()
    const Handle<WorldGrid>& GetWorldGrid() const
    {
        return m_worldGrid;
    }

    HYP_METHOD()
    const GameState& GetGameState() const;

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

    HYP_METHOD()
    void AddView(View* view);

    HYP_METHOD()
    void RemoveView(View* view);

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

    HYP_FORCE_INLINE const Array<SystemExecutionGroup*>& GetSystemExecutionGroups() const
    {
        return m_systemExecutionGroups;
    }

    /*! \brief Get Views attached to this World. Buffered so it is safe to access from either the render thread or sim thread. */
    Span<View* const> GetViews() const;

    /*! \brief Copy this frame's Views into render thread owned storage, so the sim thread is free to collect
     *  the next frame's Views while this one is still being drawn.
     *  Call from the render thread while the sim/render exclusive window is held. */
    void SnapshotViewsForRender();

    /*! \brief Gets the View responsible for collecting objects used in ray tracing. Will return nullptr if ray tracing is not enabled. */
    HYP_FORCE_INLINE View* GetRayTracingView() const
    {
        return m_rayTracingView;
    }

    /*! \brief Adds a View for processing asynchronously for this frame. */
    void ProcessViewAsync(View* view);

    void CollectScenes(Array<Scene*, SceneTempAllocator>& outScenes);
    void CollectCameras(Array<Camera*, SceneTempAllocator>& outCameras);
    void CollectViews(Array<View*, SceneTempAllocator>& outViews);
    void CollectSubsystems(Array<Subsystem*, SceneTempAllocator>& outSubsystems);

    void BeginUpdate(TaskBatch& inBatch, float delta);
    void EndUpdate();

    HYP_FIELD()
    static ScriptableDelegate<void, World*, const Handle<Scene>& /* scene */> OnSceneAdded;

    HYP_FIELD()
    static ScriptableDelegate<void, World*, Scene* /* scene */> OnSceneRemoved;

private:
    void UpdateCSMState();

    void SyncPhysicsToEntities();

    bool AddSystemToExecutionGroup(SystemBase* system);

    Handle<WorldGridLayer> GetOrCreateStreamingLayer(Name streamingLayerName);

    /// Serialization ///

    HYP_METHOD(Property = "NonStreamingScenes", Serialize)
    void DeserializeNonStreamingScenes(const Array<Handle<Scene>>& scenes);

    HYP_METHOD(Property = "NonStreamingScenes", Serialize)
    Array<Handle<Scene>> SerializeNonStreamingScenes() const;

    HYP_METHOD(Property = "StreamingLayers", Serialize, LoadOrder = 100)
    void DeserializeStreamingLayers(const Array<WGLayerDesc, DynamicAllocator>& streamingLayers);

    HYP_METHOD(Property = "StreamingLayers", Serialize, LoadOrder = 100)
    Array<WGLayerDesc, DynamicAllocator> SerializeStreamingLayers() const;

    HYP_METHOD(Property = "Systems", Serialize)
    void DeserializeSystems(const Array<Handle<SystemBase>>& systems);

    HYP_METHOD(Property = "Systems", Serialize)
    Array<Handle<SystemBase>> SerializeSystems() const;

    /// Serialization ///

    HYP_FIELD(Property = "GameInstance", Transient)
    Game* m_gameInstance;

    HYP_FIELD(Property = "WorldFlags", Serialize, LoadOrder = 0)
    EnumFlags<WorldFlags> m_worldFlags;

    HYP_FIELD(Property = "Scenes", Transient)
    Array<Handle<Scene>> m_scenes;

    HYP_FIELD(Property = "FogParams")
    FogParams m_fogParams;

    HYP_FIELD(Property = "CSMParams")
    CSMParams m_csmParams;

    HYP_FIELD(Property = "CSMState", Transient)
    CSMState m_csmState;

    // systems must load after flags are set
    HYP_FIELD(Property = "Systems", LoadOrder = 200)
    Array<Handle<SystemBase>> m_systems;

    Array<SystemExecutionGroup*> m_systemExecutionGroups;
    SystemExecutionGroup* m_rootSynchronousExecutionGroup;

    Array<View*, SceneAllocator> m_views;

    // Views, buffered so the render thread can safely read from it
    Array<View*> m_viewsPerFrame[RingBufferDepth];

    // Render thread's own copy, taken during the exclusive window. The sim starts collecting the next
    // frame's Views before this frame is submitted, so it can't read m_viewsPerFrame during passes.
    Array<View*> m_viewsRenderSnapshot;

    View* m_rayTracingView;

    SubsystemsMap m_subsystems;
    Array<Subsystem*, SceneAllocator> m_subsystemsArray;

    Handle<WorldGrid> m_worldGrid;

    Handle<PhysicsWorldBase> m_physicsWorld;

    // additional views to process for the current frame
    Array<View*, SceneAllocator> m_processViews;

    DelegateHandlerSet m_delegateHandlers;

    bool m_isInitialized;
};

} // namespace Hyperion
