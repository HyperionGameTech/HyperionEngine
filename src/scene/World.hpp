/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/Scene.hpp>
#include <scene/Subsystem.hpp>
#include <scene/GameState.hpp>

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

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

struct WGLayerDesc;

HYP_CLASS()
class HYP_API World final : public ObjectBase
{
    HYP_OBJECT_BODY(World);

public:
    using SubsystemsMap = HashMap<TypeId, Handle<Subsystem>, DynamicNodeAllocator>;

    World();
    explicit World(Name name);

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

    /*! \brief Get the placeholder Scene, used for Entities that are not attached to a Scene.
     *  This version of the function allows the caller to specify the thread the Scene uses for entity management.
     *  If the Scene does not exist for the given thread mask, it will be created.
     *\param threadId The thread the Scene should be associated with.
     * \return The handle for the detached Scene for the given thread.
     */
    const Handle<Scene>& GetDetachedScene(const ThreadId& threadId);

    HYP_FORCE_INLINE PhysicsWorld& GetPhysicsWorld()
    {
        return m_physicsWorld;
    }

    HYP_FORCE_INLINE const PhysicsWorld& GetPhysicsWorld() const
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
    void AddScene(const Handle<Scene>& scene);

    HYP_METHOD()
    bool RemoveScene(Scene* scene);

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

    /*! \brief Get Views attached to this World. Buffered so it is safe to access from either the render thread or game thread. */
    Span<View* const> GetViews() const;

    /*! \brief Gets the View responsible for collecting objects used in ray tracing. Will return nullptr if ray tracing is not enabled. */
    HYP_FORCE_INLINE View* GetRaytracingView() const
    {
        return m_raytracingView;
    }

    /*! \brief Adds a View for processing asynchronously for this frame. */
    void ProcessViewAsync(View* view);
    DelegateHandler ProcessViewAsync(View* view, Proc<void()>&& onComplete);

    /*! \brief Perform any necessary game thread specific updates to the World.
     * The main logic loop of the engine happens here. Each Scene in the World is updated,
     * and within each Scene, each Entity, etc. */
    void Update(float delta);

    Delegate<void, World*, GameStateMode, GameStateMode> OnGameStateChange;

    Delegate<void, World*, const Handle<Scene>& /* scene */> OnSceneAdded;
    Delegate<void, World*, Scene* /* scene */> OnSceneRemoved;

private:
    void Init() override;

    Handle<WorldGridLayer> GetOrCreateStreamingLayer(Name streamingLayerName);

    HYP_METHOD(Property = "Scenes", Transient)
    void SetScenes(const Array<Handle<Scene>>& scenes);

    HYP_METHOD(Property = "StreamingLayers", Serialize)
    void DeserializeStreamingLayers(const Array<WGLayerDesc, DynamicAllocator>& streamingLayers);

    HYP_METHOD(Property = "StreamingLayers", Serialize)
    Array<WGLayerDesc, DynamicAllocator> SerializeStreamingLayers() const;

    HYP_FIELD(Property = "Name", Serialize)
    Name m_name;

    HYP_FIELD(Property = "Scenes", Transient)
    Array<Handle<Scene>> m_scenes;

    Array<Handle<View>> m_views;

    // Views, buffered so the render thread can safely read from it
    Array<Array<View*>, FixedAllocator<RingBufferDepth>> m_viewsPerFrame;

    View* m_raytracingView;

    SubsystemsMap m_subsystems;
    Array<Subsystem*> m_subsystemsArray;

    Handle<WorldGrid> m_worldGrid;

    GameState m_gameState;

    TaskBatch* m_viewCollectionBatch;

    PhysicsWorld m_physicsWorld;

    // additional views to process for the current frame
    Array<View*, SceneTempAllocator> m_processViews;
};

} // namespace hyperion
