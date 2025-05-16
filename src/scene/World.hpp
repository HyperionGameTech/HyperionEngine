/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_WORLD_HPP
#define HYPERION_WORLD_HPP

#include <scene/Scene.hpp>
#include <scene/Subsystem.hpp>
#include <scene/GameState.hpp>

#include <core/object/HypObject.hpp>
#include <core/functional/Delegate.hpp>

#include <physics/PhysicsWorld.hpp>

namespace hyperion {

class WorldRenderResource;
class EditorDelegates;
struct EngineRenderStats;
class View;

struct DetachedScenesContainer
{
    World                               *world;
    HashMap<ThreadID, Handle<Scene>>    scenes;
    Mutex                               mutex;

    DetachedScenesContainer(World *world)
        : world(world)
    {
    }

    const Handle<Scene> &GetDetachedScene(const ThreadID &thread_id)
    {
        Mutex::Guard guard(mutex);

        auto it = scenes.Find(thread_id);

        if (it == scenes.End()) {
            it = scenes.Insert({
                thread_id,
                CreateSceneForThread(thread_id)
            }).first;
        }

        return it->second;
    }

private:
    Handle<Scene> CreateSceneForThread(const ThreadID &thread_id)
    {
        Handle<Scene> scene = CreateObject<Scene>(
            world,
            thread_id,
            SceneFlags::DETACHED
        );

        scene->SetName(CreateNameFromDynamicString(ANSIString("DetachedSceneForThread_") + *thread_id.GetName()));
        
        InitObject(scene);

        return scene;
    }
};

HYP_CLASS()
class HYP_API World : public HypObject<World>
{
    HYP_OBJECT_BODY(World);

public:
    World();
    World(const World &other)                   = delete;
    World &operator=(const World &other)        = delete;
    World(World &&other) noexcept               = delete;
    World &operator=(World &&other) noexcept    = delete;
    ~World();

    HYP_FORCE_INLINE WorldRenderResource &GetRenderResource() const
        { return *m_render_resource; }

    HYP_METHOD()
    EngineRenderStats *GetRenderStats() const;

    /*! \brief Get the placeholder Scene, used for Entities that are not attached to a Scene.
     *  This version of the function allows the caller to specify the thread the Scene uses for entity management.
     *  If the Scene does not exist for the given thread mask, it will be created.
     *\param thread_id The thread the Scene should be associated with.
     * \return The handle for the detached Scene for the given thread.
     */
    const Handle<Scene> &GetDetachedScene(const ThreadID &thread_id);

    HYP_FORCE_INLINE PhysicsWorld &GetPhysicsWorld()
        { return m_physics_world; }

    HYP_FORCE_INLINE const PhysicsWorld &GetPhysicsWorld() const
        { return m_physics_world; }

    template <class T, class... Args>
    HYP_FORCE_INLINE RC<T> AddSubsystem(Args &&... args)
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return AddSubsystem(TypeID::ForType<T>(), MakeRefCountedPtr<T>(std::forward<Args>(args)...)).template CastUnsafe<T>();
    }

    template <class T>
    HYP_FORCE_INLINE RC<T> AddSubsystem(const RC<T> &subsystem)
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return AddSubsystem(TypeID::ForType<T>(), subsystem).template CastUnsafe<T>();
    }

    RC<Subsystem> AddSubsystem(TypeID type_id, const RC<Subsystem> &subsystem);

    template <class T>
    HYP_FORCE_INLINE T *GetSubsystem()
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return static_cast<T *>(GetSubsystem(TypeID::ForType<T>()));
    }
    
    Subsystem *GetSubsystem(TypeID type_id) const;

    HYP_METHOD()
    Subsystem *GetSubsystemByName(WeakName name) const;

    HYP_METHOD()
    HYP_FORCE_INLINE const GameState &GetGameState() const
        { return m_game_state; }

    HYP_METHOD()
    void StartSimulating();

    HYP_METHOD()
    void StopSimulating();

    HYP_METHOD()
    void AddScene(const Handle<Scene> &scene);

    HYP_METHOD()
    bool RemoveScene(const Handle<Scene> &scene);

    /*! \brief Get the number of Scenes in the World. Must be called on the game thread.
     *  \return The number of Scenes in the World. */
    HYP_METHOD()
    bool HasScene(ID<Scene> scene_id) const;

    /*! \brief Find a Scene by its Name property. If no Scene with the given name exists, an empty handle is returned. Must be called on the game thread.
     *  \param name The name of the Scene to find.
     *  \return The Scene with the given name, or an empty handle if no Scene with the given name exists. */
    HYP_METHOD()
    const Handle<Scene> &GetSceneByName(Name name) const;

    HYP_METHOD()
    void AddView(const Handle<View> &view);

    HYP_METHOD()
    void RemoveView(const Handle<View> &view);

    HYP_FORCE_INLINE const Array<Handle<View>> &GetViews() const
        { return m_views; }

    void Init();
    
    /*! \brief Perform any necessary game thread specific updates to the World.
     * The main logic loop of the engine happens here. Each Scene in the World is updated,
     * and within each Scene, each Entity, etc. */
    void Update(GameCounter::TickUnit delta);
    
    Delegate<void, World *, GameStateMode>          OnGameStateChange;

private:
    PhysicsWorld                                    m_physics_world;

    DetachedScenesContainer                         m_detached_scenes;

    Array<Handle<Scene>>                            m_scenes;
    Array<Handle<View>>                             m_views;

    TypeMap<RC<Subsystem>>                          m_subsystems;

    GameState                                       m_game_state;

    WorldRenderResource                             *m_render_resource;
};

} // namespace hyperion

#endif
