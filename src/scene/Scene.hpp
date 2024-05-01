/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCENE_HPP
#define HYPERION_SCENE_HPP

#include <scene/Node.hpp>
#include <scene/NodeProxy.hpp>
#include <scene/Entity.hpp>
#include <scene/Octree.hpp>
#include <scene/camera/Camera.hpp>

#include <core/Base.hpp>
#include <core/threading/Scheduler.hpp>
#include <core/Containers.hpp>
#include <core/Name.hpp>

#include <rendering/rt/TLAS.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Light.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/EntityDrawCollection.hpp>

#include <math/Color.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

namespace hyperion {

class RenderEnvironment;
class World;
class Scene;
class EntityManager;

struct FogParams
{
    Color   color = Color(0xF2F8F7FF);
    float   start_distance = 250.0f;
    float   end_distance = 1000.0f;
};

template<>
struct ComponentInitInfo<STUB_CLASS(Scene)>
{
    enum Flags : ComponentFlags
    {
        SCENE_FLAGS_NONE        = 0x0,
        SCENE_FLAGS_HAS_TLAS    = 0x1,
        SCENE_FLAGS_NON_WORLD   = 0x2
    };

    ThreadMask      thread_mask = ThreadName::THREAD_GAME;
    ComponentFlags  flags = SCENE_FLAGS_NONE;
};

class Scene
    : public BasicObject<STUB_CLASS(Scene)>,
      public HasDrawProxy<STUB_CLASS(Scene)>
{
    friend class Entity;
    friend class World;
    friend class UIStage;

public:
    HYP_API Scene();
    HYP_API Scene(Handle<Camera> camera);

    HYP_API Scene(
        Handle<Camera> camera,
        const InitInfo &info
    );

    Scene(const Scene &other)               = delete;
    Scene &operator=(const Scene &other)    = delete;
    HYP_API ~Scene();

    Handle<Camera> &GetCamera()
        { return m_camera; }

    const Handle<Camera> &GetCamera() const
        { return m_camera; }

    HYP_API void SetCamera(Handle<Camera> camera);

    RenderList &GetRenderList()
        { return m_render_list; }

    const RenderList &GetRenderList() const
        { return m_render_list; }

    HYP_API NodeProxy FindNodeWithEntity(ID<Entity>) const;
    HYP_API NodeProxy FindNodeByName(const String &) const;

    /*! \brief Get the top level acceleration structure for this Scene, if it exists. */
    Handle<TLAS> &GetTLAS()
        { return m_tlas; }
    
    /*! \brief Get the top level acceleration structure for this Scene, if it exists. */
    const Handle<TLAS> &GetTLAS() const
        { return m_tlas; }

    /*! \brief Creates a top level acceleration structure for this Scene. If one already exists on this Scene,
     *  no action is performed and true is returned. If the TLAS could not be created, false is returned.
     *  The Scene must have had Init() called on it before calling this.
     */
    HYP_API bool CreateTLAS();

    NodeProxy &GetRoot()
        { return m_root_node_proxy; }

    const NodeProxy &GetRoot() const
        { return m_root_node_proxy; }

    /*! \brief Used for deserialization only */
    void SetRoot(NodeProxy &&root)
    {
        if (m_root_node_proxy) {
            m_root_node_proxy->SetScene(nullptr);
        }

        m_root_node_proxy = std::move(root);

        if (m_root_node_proxy) {
            m_root_node_proxy->SetScene(this);
        }
    }

    const RC<EntityManager> &GetEntityManager() const
        { return m_entity_manager; }

    Octree &GetOctree()
        { return m_octree; }

    const Octree &GetOctree() const
        { return m_octree; }

    RenderEnvironment *GetEnvironment() const
        { return m_environment.Get(); }

    World *GetWorld() const
        { return m_world; }

    HYP_API void SetWorld(World *world);

    /*! \brief A scene is a non-world scene if it exists not as an owner of entities,
        but rather a simple container that has items based on another Scene. For example,
        you could have a "shadow map" scene, which gathers entities from the main scene,
        but does not call Update() on them. */
    bool IsWorldScene() const
        { return !m_parent_scene.IsValid() && !m_is_non_world_scene; }

    bool IsAudioListener() const
        { return m_is_audio_listener; }

    void SetIsAudioListener(bool is_audio_listener)
        { m_is_audio_listener = is_audio_listener; }
    
    HYP_API void Init();

    /*! \brief Update the scene, including all entities, lights, etc.
        This is to be called from the GAME thread.
        You will not likely need to call this manually, as it is called
        by the World, unless you are using a Scene as a non-world scene.
        * @param delta The delta time since the last update.
    */
    HYP_API void Update(GameCounter::TickUnit delta);

    HYP_API void CollectEntities(
        RenderList &render_list, 
        const Handle<Camera> &camera,
        const Optional<RenderableAttributeSet> &override_attributes = { },
        bool skip_frustum_culling = false
    ) const;

    HYP_API void CollectDynamicEntities(
        RenderList &render_list, 
        const Handle<Camera> &camera,
        const Optional<RenderableAttributeSet> &override_attributes = { },
        bool skip_frustum_culling = false
    ) const;

    HYP_API void CollectStaticEntities(
        RenderList &render_list, 
        const Handle<Camera> &camera,
        const Optional<RenderableAttributeSet> &override_attributes = { },
        bool skip_frustum_culling = false
    ) const;

private:
    void EnqueueRenderUpdates();

    Handle<Camera>                  m_camera;
    RenderList                      m_render_list;

    UniquePtr<RenderEnvironment>    m_environment;
    World                           *m_world;

    FogParams                       m_fog_params;

    NodeProxy                       m_root_node_proxy;
    RC<EntityManager>               m_entity_manager;

    Octree                          m_octree;

    Handle<TLAS>                    m_tlas;

    Matrix4                         m_last_view_projection_matrix;

    Handle<Scene>                   m_parent_scene;
    bool                            m_is_non_world_scene;

    bool                            m_is_audio_listener;
                                 
    mutable DataMutationState         m_mutation_state;
};

} // namespace hyperion

#endif
