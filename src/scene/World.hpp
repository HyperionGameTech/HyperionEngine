/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/Scene.hpp>
#include <scene/Subsystem.hpp>
#include <scene/GameState.hpp>

#include <core/object/HypObject.hpp>

#include <core/functional/Delegate.hpp>
#include <core/functional/Proc.hpp>

#include <physics/PhysicsWorld.hpp>

namespace hyperion {

class EditorDelegates;
struct RenderStats;
class View;
class WorldGrid;

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

struct DetachedScenesContainer
{
    World* world;
    HashMap<ThreadId, Handle<Scene>> scenes;
    Mutex mutex;

    DetachedScenesContainer(World* world)
        : world(world)
    {
    }

    const Handle<Scene>& GetDetachedScene(const ThreadId& threadId)
    {
        Mutex::Guard guard(mutex);

        auto it = scenes.Find(threadId);

        if (it == scenes.End())
        {
            it = scenes.Insert({ threadId, CreateSceneForThread(threadId) }).first;
        }

        return it->second;
    }

private:
    Handle<Scene> CreateSceneForThread(const ThreadId& threadId)
    {
        Handle<Scene> scene = CreateObject<Scene>(world, threadId, SceneFlags::DETACHED);
        scene->SetName(CreateNameFromDynamicString(ANSIString("DetachedSceneForThread_") + *threadId.GetName()));

        InitObject(scene);

        return scene;
    }
};

HYP_CLASS()
class HYP_API World final : public HypObjectBase
{
    HYP_OBJECT_BODY(World);

public:
    World();
    World(const World& other) = delete;
    World& operator=(const World& other) = delete;
    World(World&& other) noexcept = delete;
    World& operator=(World&& other) noexcept = delete;
    ~World() override;

    HYP_METHOD()
    RenderStats* GetRenderStats() const;

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
    HYP_FORCE_INLINE Handle<T> AddSubsystem()
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return ObjCast<T>(AddSubsystem(TypeId::ForType<T>(), CreateObject<T>()));
    }

    template <class T>
    HYP_FORCE_INLINE Handle<T> AddSubsystem(const Handle<T>& subsystem)
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return ObjCast<T>(AddSubsystem(TypeId::ForType<T>(), subsystem));
    }

    Handle<Subsystem> AddSubsystem(TypeId typeId, const Handle<Subsystem>& subsystem);

    template <class T>
    HYP_FORCE_INLINE T* GetSubsystem()
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return ObjCast<T>(GetSubsystem(TypeId::ForType<T>()));
    }

    Subsystem* GetSubsystem(TypeId typeId) const;

    HYP_METHOD()
    Subsystem* GetSubsystemByName(WeakName name) const;

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
    bool RemoveScene(const Handle<Scene>& scene);

    /*! \brief Get the number of Scenes in the World. Must be called on the game thread.
     *  \return The number of Scenes in the World. */
    HYP_METHOD()
    bool HasScene(ObjId<Scene> sceneId) const;

    /*! \brief Find a Scene by its Name property. If no Scene with the given name exists, an empty handle is returned. Must be called on the game thread.
     *  \param name The name of the Scene to find.
     *  \return The Scene with the given name, or an empty handle if no Scene with the given name exists. */
    HYP_METHOD()
    const Handle<Scene>& GetSceneByName(Name name) const;

    HYP_METHOD()
    const Array<Handle<Scene>>& GetScenes() const
    {
        return m_scenes;
    }

    HYP_METHOD()
    void AddView(const Handle<View>& view);

    HYP_METHOD()
    void RemoveView(const Handle<View>& view);

    /*! \brief Get Views attached to this World. Buffered so it is safe to access from either the render thread or game thread. */
    Span<const Handle<View>> GetViews() const;

    /*! \brief Gets the View responsible for collecting objects used in ray tracing. Will return nullptr if ray tracing is not enabled. */
    HYP_FORCE_INLINE View* GetRaytracingView() const
    {
        return m_raytracingView;
    }

    /*! \brief Adds a View for processing asynchronously for this frame. */
    void ProcessViewAsync(const Handle<View>& view);
    DelegateHandler ProcessViewAsync(const Handle<View>& view, Proc<void()>&& onComplete);

    /*! \brief Perform any necessary game thread specific updates to the World.
     * The main logic loop of the engine happens here. Each Scene in the World is updated,
     * and within each Scene, each Entity, etc. */
    void Update(float delta);

    Delegate<void, World*, GameStateMode, GameStateMode> OnGameStateChange;

    Delegate<void, World*, const Handle<Scene>& /* scene */> OnSceneAdded;
    Delegate<void, World*, const Handle<Scene>& /* scene */> OnSceneRemoved;

private:
    void Init() override;

    PhysicsWorld m_physicsWorld;

    DetachedScenesContainer m_detachedScenes;

    Array<Handle<Scene>> m_scenes;
    Array<Handle<View>> m_views;

    // Views, buffered so the render thread can safely read from it
    Array<Array<Handle<View>>, FixedAllocator<g_tripleBuffer ? 3 : 2>> m_viewsPerFrame;

    View* m_raytracingView;

    TypeMap<Handle<Subsystem>> m_subsystems;

    Handle<WorldGrid> m_worldGrid;

    GameState m_gameState;

    TaskBatch* m_viewCollectionBatch;

    // additional views to process for the current frame
    Array<Handle<View>> m_processViews;
};

} // namespace hyperion

