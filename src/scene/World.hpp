#ifndef HYPERION_V2_WORLD_H
#define HYPERION_V2_WORLD_H

#include <scene/Scene.hpp>
#include <physics/PhysicsWorld.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <core/lib/AtomicSemaphore.hpp>

#include <mutex>
#include <atomic>

namespace hyperion::v2 {

using renderer::Frame;

struct RenderListNode
{
    RenderList *render_list = nullptr;
    RenderListNode *prev = nullptr;
    RenderListNode *next = nullptr;
};

class RenderListContainer
{
public:
    RenderListContainer()
        : m_num_render_lists { 0u }
    {
    }

    RenderListContainer(const RenderListContainer &other) = delete;
    RenderListContainer &operator=(RenderListContainer &other) = delete;
    RenderListContainer(RenderListContainer &&other) noexcept = delete;
    RenderListContainer &operator=(RenderListContainer &&other) noexcept = delete;
    ~RenderListContainer() = default;

    UInt NumRenderLists() const
        { return m_num_render_lists.load(); }

    void AddScene(const Scene *scene)
    {
        AssertThrow(scene != nullptr);
        AssertThrowMsg(scene->GetCamera().IsValid(), "Cannot acquire RenderList for Scene with no Camera attached.");

        const UInt scene_index = scene->GetID().ToIndex();

        m_render_lists_by_id_index[scene_index].SetCamera(scene->GetCamera());

        const UInt render_list_index = m_num_render_lists.fetch_add(1u);
        m_render_lists[render_list_index] = &m_render_lists_by_id_index[scene_index];
    }

    void RemoveScene(const Scene *scene)
    {
        AssertThrow(scene != nullptr);

        const UInt scene_index = scene->GetID().ToIndex();
        
        m_num_render_lists.fetch_sub(1u);
        m_render_lists_by_id_index[scene_index].SetCamera(Handle<Camera>::empty);
        m_render_lists_by_id_index[scene_index].Reset();
    }

    RenderList &GetRenderListForScene(ID<Scene> scene_id)
        { return m_render_lists_by_id_index[scene_id.ToIndex()]; }

    const RenderList &GetRenderListForScene(ID<Scene> scene_id) const
        { return m_render_lists_by_id_index[scene_id.ToIndex()]; }

    RenderList *GetRenderListAtIndex(UInt index) const
        { return m_render_lists[index]; }

private:
    FixedArray<RenderList *, max_scenes> m_render_lists;
    FixedArray<RenderList, max_scenes> m_render_lists_by_id_index;
    std::atomic<UInt> m_num_render_lists;
};

class World : public BasicObject<STUB_CLASS(World)>
{
public:
    World();
    World(const World &other) = delete;
    World &operator=(const World &other) = delete;
    ~World();

    Octree &GetOctree() { return m_octree; }
    const Octree &GetOctree() const { return m_octree; }

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
     * and within each Scene, each Entity, etc.
     */
    void Update(GameCounter::TickUnit delta);

    /*! \brief Perform any necessary render thread specific updates to the World. */
    void PreRender(Frame *frame);
    void Render(Frame *frame);

private:
    void PerformSceneUpdates();

    Octree m_octree;
    PhysicsWorld m_physics_world;
    RenderListContainer m_render_list_container;

    FlatSet<Handle<Scene>> m_scenes;
    FlatSet<Handle<Scene>> m_scenes_pending_addition;
    FlatSet<Handle<Scene>> m_scenes_pending_removal;

    std::atomic_bool m_has_scene_updates { false };
    BinarySemaphore m_scene_update_sp;
    std::mutex m_scene_update_mutex;
};

} // namespace hyperion::v2

#endif
