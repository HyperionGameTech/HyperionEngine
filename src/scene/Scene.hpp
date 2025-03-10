/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCENE_HPP
#define HYPERION_SCENE_HPP

#include <scene/Node.hpp>
#include <scene/NodeProxy.hpp>
#include <scene/Entity.hpp>
#include <scene/Octree.hpp>
#include <scene/camera/Camera.hpp>

#include <core/Base.hpp>
#include <core/Name.hpp>

#include <core/utilities/DataMutationState.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/Shader.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/Color.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Scene);

class RenderEnvironment;
class World;
class Scene;
class EntityManager;
class WorldGrid;
class SceneRenderResource;

struct FogParams
{
    Color   color = Color(0xF2F8F7FF);
    float   start_distance = 250.0f;
    float   end_distance = 1000.0f;
};

enum class SceneFlags : uint32
{
    NONE        = 0x0,
    FOREGROUND  = 0x1,
    DETACHED    = 0x2,
    HAS_TLAS    = 0x4,
    UI          = 0x8
};

HYP_MAKE_ENUM_FLAGS(SceneFlags);

struct SceneDrawProxy
{
    uint32 frame_counter;
};

HYP_CLASS()
class HYP_API Scene : public HypObject<Scene>
{
    friend class World;
    friend class UIStage;

    HYP_OBJECT_BODY(Scene);

public:
    Scene();
    
    Scene(EnumFlags<SceneFlags> flags);
    
    Scene(
        World *world,
        EnumFlags<SceneFlags> flags = SceneFlags::NONE
    );

    Scene(
        World *world,
        const Handle<Camera> &camera,
        EnumFlags<SceneFlags> flags = SceneFlags::NONE
    );

    Scene(
        World *world,
        const Handle<Camera> &camera,
        ThreadID owner_thread_id,
        EnumFlags<SceneFlags> flags = SceneFlags::NONE
    );

    Scene(const Scene &other)               = delete;
    Scene &operator=(const Scene &other)    = delete;
    ~Scene();

    HYP_FORCE_INLINE SceneRenderResource &GetRenderResource() const
        { return *m_render_resource; }

    /*! \brief Get the thread ID that owns this Scene. */
    HYP_FORCE_INLINE ThreadID GetOwnerThreadID() const
        { return m_owner_thread_id; }

    /*! \brief Set the thread ID that owns this Scene.
     *  This is used to assert that the Scene is being accessed from the correct thread.
     *  \note Only call this if you know what you are doing. */
    void SetOwnerThreadID(ThreadID owner_thread_id);

    HYP_FORCE_INLINE EnumFlags<SceneFlags> GetFlags() const
        { return m_flags; }

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetName() const
        { return m_name; }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetName(Name name)
        { m_name = name; }

    /*! \brief Get the camera that is used to render this Scene and perform frustum culling. */
    HYP_METHOD(Property="Camera", Serialize=true, Editor=true)
    HYP_FORCE_INLINE const Handle<Camera> &GetCamera() const
        { return m_camera; }

    /*! \brief Set the camera that is used to render this Scene. */
    HYP_METHOD(Property="Camera", Serialize=true, Editor=true)
    void SetCamera(const Handle<Camera> &camera);

    HYP_FORCE_INLINE RenderCollector &GetRenderCollector()
        { return m_render_collector; }

    HYP_FORCE_INLINE const RenderCollector &GetRenderCollector() const
        { return m_render_collector; }

    HYP_METHOD()
    HYP_NODISCARD NodeProxy FindNodeWithEntity(ID<Entity> entity) const;

    HYP_METHOD()
    HYP_NODISCARD NodeProxy FindNodeByName(UTF8StringView name) const;
    
    /*! \brief Get the top level acceleration structure for this Scene, if it exists. */
    HYP_FORCE_INLINE const TLASRef &GetTLAS() const
        { return m_tlas; }

    /*! \brief Creates a top level acceleration structure for this Scene. If one already exists on this Scene,
     *  no action is performed and true is returned. If the TLAS could not be created, false is returned.
     *  The Scene must have had Init() called on it before calling this.
     */
    bool CreateTLAS();
    
    HYP_METHOD(Property="Root", Serialize=true, Editor=true)
    HYP_FORCE_INLINE const NodeProxy &GetRoot() const
        { return m_root_node_proxy; }

    HYP_METHOD(Property="Root", Serialize=true, Editor=true)
    void SetRoot(const NodeProxy &root);

    HYP_METHOD()
    HYP_FORCE_INLINE const RC<EntityManager> &GetEntityManager() const
        { return m_entity_manager; }

    HYP_FORCE_INLINE Octree &GetOctree()
        { return m_octree; }

    HYP_FORCE_INLINE const Octree &GetOctree() const
        { return m_octree; }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsAttachedToWorld() const
        { return m_world != nullptr; }

    HYP_METHOD()
    HYP_FORCE_INLINE World *GetWorld() const
        { return m_world; }

    void SetWorld(World *world);

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsForegroundScene() const
        { return m_flags & SceneFlags::FOREGROUND; }

    HYP_FORCE_INLINE bool IsBackgroundScene() const
    { return !(m_flags & SceneFlags::FOREGROUND); }

    HYP_METHOD(Property="IsAudioListener", Serialize=true)
    HYP_FORCE_INLINE bool IsAudioListener() const
        { return m_is_audio_listener; }

    HYP_METHOD(Property="IsAudioListener", Serialize=true)
    HYP_FORCE_INLINE void SetIsAudioListener(bool is_audio_listener)
        { m_is_audio_listener = is_audio_listener; }

    HYP_FORCE_INLINE const SceneDrawProxy &GetProxy() const
        { return m_proxy; }

    WorldGrid *GetWorldGrid() const;
    
    void Init();

    void Update(GameCounter::TickUnit delta);

    RenderCollector::CollectionResult CollectEntities(
        RenderCollector &render_collector, 
        const Handle<Camera> &camera,
        const Optional<RenderableAttributeSet> &override_attributes = { },
        bool skip_frustum_culling = false
    ) const;

    RenderCollector::CollectionResult CollectDynamicEntities(
        RenderCollector &render_collector, 
        const Handle<Camera> &camera,
        const Optional<RenderableAttributeSet> &override_attributes = { },
        bool skip_frustum_culling = false
    ) const;

    RenderCollector::CollectionResult CollectStaticEntities(
        RenderCollector &render_collector, 
        const Handle<Camera> &camera,
        const Optional<RenderableAttributeSet> &override_attributes = { },
        bool skip_frustum_culling = false
    ) const;

    bool AddToWorld(World *world);
    bool RemoveFromWorld();
    
    void EnqueueRenderUpdates();

private:
    template <class SystemType>
    void AddSystemIfApplicable();

    HYP_FIELD(Property="Name", Serialize=true, Editor=true)
    Name                        m_name;

    HYP_FIELD(Property="Flags", Serialize=true)
    EnumFlags<SceneFlags>       m_flags;

    NodeProxy                   m_root_node_proxy;

    ThreadID                    m_owner_thread_id;

    World                       *m_world;

    Handle<Camera>              m_camera;
    RenderCollector             m_render_collector;

    FogParams                   m_fog_params;

    RC<EntityManager>           m_entity_manager;

    Octree                      m_octree;

    TLASRef                     m_tlas;

    bool                        m_is_audio_listener;

    GameCounter::TickUnit       m_previous_delta;

    SceneDrawProxy              m_proxy;

    SceneRenderResource         *m_render_resource;
};

} // namespace hyperion

#endif
