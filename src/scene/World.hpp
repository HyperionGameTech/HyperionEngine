/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/Scene.hpp>
#include <scene/Subsystem.hpp>
#include <scene/GameState.hpp>
#include <scene/SystemExecutionGroup.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/functional/Delegate.hpp>
#include <core/functional/Proc.hpp>

#include <physics/PhysicsWorld.hpp>

#include <engine/EngineMemory.hpp>

#include <core/memory/allocator/ArenaAllocator.hpp>
#include <core/memory/pool/Pool.hpp>

namespace hyperion {

class EditorDelegates;
class View;
class WorldGrid;
class WorldGridLayer;
class PhysicsWorldBase;

enum GlobalRendererType : uint32;

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

HYP_CLASS()
class HYP_API World final : public ObjectBase
{
    HYP_OBJECT_BODY(World);

public:
    using SubsystemsMap = HashMap<TypeId, Handle<Subsystem>, DynamicNodeAllocator>;

    World();
    explicit World(Name name, EnumFlags<WorldFlags> worldFlags = WorldFlags::DEFAULT);

    World(const World& other) = delete;
    World& operator=(const World& other) = delete;

    World(World&& other) noexcept = delete;
    World& operator=(World&& other) noexcept = delete;

    ~World() override;

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetName(Name name)
    {
        m_name = name;
    }

    HYP_METHOD(Property = "WorldFlags", Serialize)
    HYP_FORCE_INLINE EnumFlags<WorldFlags> GetWorldFlags() const
    {
        return m_worldFlags;
    }

    HYP_METHOD(Property = "WorldFlags", Serialize)
    void SetWorldFlags(EnumFlags<WorldFlags> flags);

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

        return ObjCast<T>(AddSubsystem(TypeId::ForType<T>(), CreateObject<T>()));
    }

    template <class T>
    HYP_FORCE_INLINE const Handle<T>& AddSubsystem(const Handle<T>& subsystem)
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return ObjCast<T>(AddSubsystem(TypeId::ForType<T>(), subsystem));
    }

    const Handle<Subsystem>& AddSubsystem(TypeId typeId, const Handle<Subsystem>& subsystem);

    template <class T>
    HYP_FORCE_INLINE T* GetSubsystem()
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return ObjCast<T>(GetSubsystem(TypeId::ForType<T>()));
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
    HYP_FORCE_INLINE const GameState& GetGameState() const
    {
        return m_gameState;
    }

    HYP_METHOD()
    void StartSimulating();

    HYP_METHOD()
    void StopSimulating();

    HYP_METHOD()
    void AddScene(const Handle<Scene>& scene, bool addToStreamingLayer = true);

    HYP_METHOD()
    bool RemoveScene(Scene* scene, bool removeFromStreamingLayer = true);

    /*! \brief Get the number of Scenes in the World. Must be called on the game thread.
     *  \return The number of Scenes in the World. */
    HYP_METHOD()
    bool HasScene(ObjId<Scene> sceneId) const;

    /*! \brief Find a Scene by its Name property. If no Scene with the given name exists, an empty handle is returned. Must be called on the game thread.
     *  \param name The name of the Scene to find.
     *  \return The Scene with the given name, or an empty handle if no Scene with the given name exists. */
    HYP_METHOD()
    const Handle<Scene>& GetSceneByName(Name name) const;

    Span<const Handle<Scene>> GetScenes() const
    {
        return m_scenes;
    }

    HYP_METHOD()
    void AddView(const Handle<View>& view);

    HYP_METHOD()
    void RemoveView(View* view);

    /*! \brief Adds a System to the World.
     *  \param[in] system The System to add.
     */
    SystemBase* AddSystem(const Handle<SystemBase>& system);

    template <class SystemType>
    HYP_FORCE_INLINE SystemType* GetSystem() const
    {
        static const TypeId typeId = TypeId::ForType<SystemType>();

        for (const SystemExecutionGroup& group : m_systemExecutionGroups)
        {
            if (group.HasSystem<SystemType>())
            {
                return group.GetSystem<SystemType>();
            }
        }

        return nullptr;
    }

    HYP_FORCE_INLINE Array<SystemExecutionGroup>& GetSystemExecutionGroups()
    {
        return m_systemExecutionGroups;
    }

    HYP_FORCE_INLINE const Array<SystemExecutionGroup>& GetSystemExecutionGroups() const
    {
        return m_systemExecutionGroups;
    }

    /*! \brief Get Views attached to this World. Buffered so it is safe to access from either the render thread or game thread. */
    Span<View* const> GetViews() const;

    /*! \brief Gets the View responsible for collecting objects used in ray tracing. Will return nullptr if ray tracing is not enabled. */
    HYP_FORCE_INLINE View* GetRaytracingView() const
    {
        return m_raytracingView;
    }

    /*! \brief Adds a View for processing asynchronously for this frame. */
    void ProcessViewAsync(View* view);

    void CollectViews(Array<View*, SceneAllocator>& outViews);
    void CollectSubsystems(Array<Subsystem*, SceneAllocator>& outSubsystems);

    void BeginUpdate(TaskBatch& inBatch, float delta);
    void EndUpdate();

    Delegate<void, World*, GameStateMode, GameStateMode> OnGameStateChange;

    Delegate<void, World*, const Handle<Scene>& /* scene */> OnSceneAdded;
    Delegate<void, World*, Scene* /* scene */> OnSceneRemoved;

private:
    void Init() override;

    void UpdateDirtyMeshEntities();

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

    /// Serialization ///

    HYP_FIELD(Property = "Name", Serialize)
    Name m_name;

    HYP_FIELD(Property = "WorldFlags", Serialize)
    EnumFlags<WorldFlags> m_worldFlags;

    HYP_FIELD(Property = "Scenes", Transient)
    Array<Handle<Scene>> m_scenes;

    Array<SystemExecutionGroup> m_systemExecutionGroups;
    SystemExecutionGroup* m_rootSynchronousExecutionGroup;

    Array<Handle<View>> m_views;

    // Views, buffered so the render thread can safely read from it
    Array<Array<View*>, FixedAllocator<RingBufferDepth>> m_viewsPerFrame;

    View* m_raytracingView;

    SubsystemsMap m_subsystems;
    Array<Subsystem*> m_subsystemsArray;

    Handle<WorldGrid> m_worldGrid;

    GameState m_gameState;

    Handle<PhysicsWorldBase> m_physicsWorld;

    // additional views to process for the current frame
    Array<View*, SceneTempAllocator> m_processViews;

    DelegateHandlerSet m_delegateHandlers;
};

} // namespace hyperion
