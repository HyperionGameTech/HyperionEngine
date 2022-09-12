#ifndef HYPERION_V2_SCENE_H
#define HYPERION_V2_SCENE_H

#include "Node.hpp"
#include "NodeProxy.hpp"
#include "Entity.hpp"
#include "Octree.hpp"
#include <rendering/Base.hpp>
#include <rendering/rt/TLAS.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Light.hpp>
#include <core/Scheduler.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <camera/Camera.hpp>
#include <GameCounter.hpp>
#include <Types.hpp>
#include <unordered_map>

namespace hyperion::v2 {

class RenderEnvironment;
class World;

class Scene : public EngineComponentBase<STUB_CLASS(Scene)>
{
    friend class Entity;
    friend class World;
public:
    static constexpr UInt32 max_environment_textures = SceneShaderData::max_environment_textures;

    Scene(Handle<Camera> &&camera);
    Scene(const Scene &other) = delete;
    Scene &operator=(const Scene &other) = delete;
    ~Scene();

    Handle<Camera> &GetCamera() { return m_camera; }
    const Handle<Camera> &GetCamera() const { return m_camera; }
    void SetCamera(Handle<Camera> &&camera) { m_camera = std::move(camera); }

    /*! \brief Add an Entity to the queue. On Update(), it will be added to the scene. */
    bool AddEntity(Handle<Entity> &&entity);
    bool HasEntity(Entity::ID id) const;
    /*! \brief Remove an Entity from the Scene in an enqueued way. On Update(), it will be removed from the scene. */
    bool RemoveEntity(const Handle<Entity> &entity);

    /*! \brief Get the top level acceleration structure for this Scene, if it exists. */
    Handle<TLAS> &GetTLAS()
        { return m_tlas; }
    
    /*! \brief Get the top level acceleration structure for this Scene, if it exists. */
    const Handle<TLAS> &GetTLAS() const
        { return m_tlas; }

    /*! \brief Creates a top level acceleration structure for this Scene. If one already exists on this Scene,
     *  no action is performed and true is returned. If the TLAS could not be created, false is returned.
     *  The Scene must be in a READY state to call this.
     */
    bool CreateTLAS();

    /* ONLY CALL FROM GAME THREAD!!! */
    auto &GetEntities() { return m_entities; }
    const auto &GetEntities() const { return m_entities; }

    NodeProxy &GetRoot() { return m_root_node_proxy; }
    const NodeProxy &GetRoot() const { return m_root_node_proxy; }

    RenderEnvironment *GetEnvironment() const { return m_environment; }

    World *GetWorld() const { return m_world; }
    void SetWorld(World *world);
    
    void Init(Engine *engine);

    void ForceUpdate();

    BoundingBox m_aabb;

private:
    // World only calls
    void Update(
        Engine *engine,
        GameCounter::TickUnit delta,
        bool update_octree_visiblity = true
    );

    void EnqueueRenderUpdates();
    
    void AddPendingEntities();
    void RemovePendingEntities();

    void RequestRendererInstanceUpdate(Handle<Entity> &entity);
    void RemoveFromRendererInstance(Handle<Entity> &entity, RendererInstance *renderer_instance);
    void RemoveFromRendererInstances(Handle<Entity> &entity);

    Handle<Camera> m_camera;
    NodeProxy m_root_node_proxy;
    RenderEnvironment *m_environment;
    World *m_world;

    // entities live in GAME thread
    FlatMap<IDBase, Handle<Entity>> m_entities;

    // NOTE: not for thread safety, it's to defer updates so we don't
    // remove in the update loop.
    FlatSet<Entity::ID> m_entities_pending_removal;
    FlatSet<Handle<Entity>> m_entities_pending_addition;

    // TODO: Move to RenderEnvironment
    Handle<TLAS> m_tlas;

    Matrix4 m_last_view_projection_matrix;
                                 
    mutable ShaderDataState m_shader_data_state;
};

} // namespace hyperion::v2

#endif
