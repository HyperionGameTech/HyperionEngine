/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_WORLD_HPP
#define HYPERION_WORLD_HPP

#include <scene/Scene.hpp>

#include <physics/PhysicsWorld.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

class RenderListContainer
{
public:
    RenderListContainer()
        : m_num_render_lists { 0u }
    {
    }

    RenderListContainer(const RenderListContainer &other)                   = delete;
    RenderListContainer &operator=(RenderListContainer &other)              = delete;
    RenderListContainer(RenderListContainer &&other) noexcept               = delete;
    RenderListContainer &operator=(RenderListContainer &&other) noexcept    = delete;
    ~RenderListContainer()                                                  = default;

    uint NumRenderLists() const
        { return m_num_render_lists.Get(MemoryOrder::ACQUIRE); }

    void AddScene(const Scene *scene)
    {
        AssertThrow(scene != nullptr);
        AssertThrowMsg(scene->GetCamera().IsValid(), "Cannot acquire RenderList for Scene with no Camera attached.");

        const uint scene_index = scene->GetID().ToIndex();

        m_render_lists_by_id_index[scene_index].SetCamera(scene->GetCamera());

        const uint render_list_index = m_num_render_lists.Increment(1u, MemoryOrder::ACQUIRE_RELEASE);
        m_render_lists[render_list_index] = &m_render_lists_by_id_index[scene_index];
    }

    void RemoveScene(const Scene *scene)
    {
        AssertThrow(scene != nullptr);

        const uint scene_index = scene->GetID().ToIndex();
        
        m_num_render_lists.Decrement(1u, MemoryOrder::RELEASE);
        m_render_lists_by_id_index[scene_index].SetCamera(Handle<Camera>::empty);
        m_render_lists_by_id_index[scene_index].Reset();
    }

    RenderList &GetRenderListForScene(ID<Scene> scene_id)
        { return m_render_lists_by_id_index[scene_id.ToIndex()]; }

    const RenderList &GetRenderListForScene(ID<Scene> scene_id) const
        { return m_render_lists_by_id_index[scene_id.ToIndex()]; }

    RenderList *GetRenderListAtIndex(uint index) const
        { return m_render_lists[index]; }

private:
    FixedArray<RenderList *, max_scenes>    m_render_lists;
    FixedArray<RenderList, max_scenes>      m_render_lists_by_id_index;
    AtomicVar<uint32>                       m_num_render_lists;
};

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

            const ThreadID thread_id = Threads::GetThreadID(thread_name);

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
            Handle<Camera>::empty,
            thread_id
        );

        scene->SetName(CreateNameFromDynamicString(ANSIString("DetachedSceneForThread_") + *thread_id.name));
        scene->SetWorld(world);
        InitObject(scene);

        return scene;
    }
};

class HYP_API World : public BasicObject<World>
{
public:
    World();
    World(const World &other)               = delete;
    World &operator=(const World &other)    = delete;
    ~World();

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

    PhysicsWorld &GetPhysicsWorld() { return m_physics_world; }
    const PhysicsWorld &GetPhysicsWorld() const { return m_physics_world; }

    RenderListContainer &GetRenderListContainer()
        { return m_render_list_container; }

    const RenderListContainer &GetRenderListContainer() const
        { return m_render_list_container; }

    void AddScene(const Handle<Scene> &scene);
    void AddScene(Handle<Scene> &&scene);
    void RemoveScene(ID<Scene> id);

    void Init();
    
    /*! \brief Perform any necessary game thread specific updates to the World.
     * The main logic loop of the engine happens here. Each Scene in the World is updated,
     * and within each Scene, each Entity, etc. */
    void Update(GameCounter::TickUnit delta);

    /*! \brief Perform any necessary render thread specific updates to the World. */
    void PreRender(Frame *frame);
    void Render(Frame *frame);

private:
    void PerformSceneUpdates();

    PhysicsWorld                        m_physics_world;
    RenderListContainer                 m_render_list_container;

    // FlatMap<ThreadID, Handle<Scene>>    m_detached_scenes;
    // Mutex                               m_detached_scenes_mutex;

    DetachedScenesContainer             m_detached_scenes;

    FlatSet<Handle<Scene>>              m_scenes;
    FlatSet<Handle<Scene>>              m_scenes_pending_addition;
    FlatSet<Handle<Scene>>              m_scenes_pending_removal;

    AtomicVar<bool>                     m_has_scene_updates { false };
    Mutex                               m_scene_update_mutex;
};

} // namespace hyperion

#endif
