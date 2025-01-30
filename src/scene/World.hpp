/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_WORLD_HPP
#define HYPERION_WORLD_HPP

#include <scene/Scene.hpp>
#include <scene/Subsystem.hpp>
#include <scene/GameState.hpp>

#include <core/object/HypObject.hpp>
#include <core/functional/Delegate.hpp>

#include <physics/PhysicsWorld.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

class WorldRenderResources;
class EditorDelegates;

struct DetachedScenesContainer
{
    World                                                                                   *world;
    FixedArray<Handle<Scene>, MathUtil::FastLog2_Pow2(uint64(ThreadName::THREAD_STATIC))>   static_thread_scenes;
    HashMap<ThreadID, Handle<Scene>>                                                        dynamic_thread_scenes;
    Mutex                                                                                   dynamic_thread_scenes_mutex;

    DetachedScenesContainer(World *world)
        : world(world)
    {
        for (uint64 i = 0; i < static_thread_scenes.Size(); i++) {
            const uint64 thread_name_value = 1ull << i;
            const ThreadName thread_name = ThreadName(thread_name_value);

            const ThreadID thread_id = Threads::GetStaticThreadID(thread_name);

            if (!thread_id.IsValid()) {
                continue;
            }

            AssertThrow(!thread_id.IsDynamic());

            static_thread_scenes[i] = CreateSceneForThread(thread_id);
        }
    }

    const Handle<Scene> &GetDetachedScene(ThreadID thread_id)
    {
        if (thread_id.IsDynamic()) {
            Mutex::Guard guard(dynamic_thread_scenes_mutex);

            auto it = dynamic_thread_scenes.Find(thread_id);

            if (it == dynamic_thread_scenes.End()) {
                it = dynamic_thread_scenes.Insert({
                    thread_id,
                    CreateSceneForThread(thread_id)
                }).first;
            }

            return it->second;
        }

#ifdef HYP_DEBUG_MODE
        AssertThrow(MathUtil::IsPowerOfTwo(thread_id.value));
        AssertThrow(MathUtil::FastLog2_Pow2(thread_id.value) < static_thread_scenes.Size());
#endif

        return static_thread_scenes[MathUtil::FastLog2_Pow2(thread_id.value)];
    }

private:
    Handle<Scene> CreateSceneForThread(ThreadID thread_id)
    {
        Handle<Scene> scene = CreateObject<Scene>(
            world,
            Handle<Camera>::empty,
            thread_id,
            SceneFlags::DETACHED
        );

        scene->SetName(CreateNameFromDynamicString(ANSIString("DetachedSceneForThread_") + *thread_id.name));
        
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

    HYP_FORCE_INLINE WorldRenderResources &GetRenderResources() const
        { return *m_render_resources; }

    /*! \brief Get the placeholder Scene, used for Entities that are not attached to a Scene.
     *  This version of the function allows the caller to specify the thread the Scene uses for entity management.
     *  If the Scene does not exist for the given thread mask, it will be created.
     *\param thread_name The thread the Scene should be associated with.
     * \return The handle for the detached Scene for the given thread.
     */
    const Handle<Scene> &GetDetachedScene(ThreadName thread_name = ThreadName::THREAD_GAME);

    /*! \brief Get the placeholder Scene, used for Entities that are not attached to a Scene.
     *  This version of the function allows the caller to specify the thread the Scene uses for entity management.
     *  If the Scene does not exist for the given thread mask, it will be created.
     *\param thread_id The thread the Scene should be associated with.
     * \return The handle for the detached Scene for the given thread.
     */
    const Handle<Scene> &GetDetachedScene(ThreadID thread_id);

    HYP_FORCE_INLINE PhysicsWorld &GetPhysicsWorld()
        { return m_physics_world; }

    HYP_FORCE_INLINE const PhysicsWorld &GetPhysicsWorld() const
        { return m_physics_world; }

    template <class T, class... Args>
    HYP_FORCE_INLINE T *AddSubsystem(Args &&... args)
    {
        static_assert(std::is_base_of_v<Subsystem, T>, "T must be a subclass of Subsystem");

        return static_cast<T *>(AddSubsystem(TypeID::ForType<T>(), MakeRefCountedPtr<T>(std::forward<Args>(args)...)));
    }

    Subsystem *AddSubsystem(TypeID type_id, RC<Subsystem> &&subsystem);

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

    void AddScene(const Handle<Scene> &scene);
    bool RemoveScene(const WeakHandle<Scene> &scene);

    /*! \brief Get the number of Scenes in the World. Must be called on the game thread.
     *  \return The number of Scenes in the World. */
    HYP_METHOD()
    bool HasScene(ID<Scene> scene_id) const;

    /*! \brief Find a Scene by its Name property. If no Scene with the given name exists, an empty handle is returned. Must be called on the game thread.
     *  \param name The name of the Scene to find.
     *  \return The Scene with the given name, or an empty handle if no Scene with the given name exists. */
    HYP_METHOD()
    const Handle<Scene> &GetSceneByName(Name name) const;

    void Init();
    
    /*! \brief Perform any necessary game thread specific updates to the World.
     * The main logic loop of the engine happens here. Each Scene in the World is updated,
     * and within each Scene, each Entity, etc. */
    void Update(GameCounter::TickUnit delta);
    
    Delegate<void, World *, GameStateMode>  OnGameStateChange;

private:
    PhysicsWorld                            m_physics_world;

    DetachedScenesContainer                 m_detached_scenes;

    Array<Handle<Scene>>                    m_scenes;

    TypeMap<RC<Subsystem>>                  m_subsystems;

    GameState                               m_game_state;

    WorldRenderResources                    *m_render_resources;
};

} // namespace hyperion

#endif
