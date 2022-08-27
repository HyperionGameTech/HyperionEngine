#ifndef HYPERION_V2_SPATIAL_H
#define HYPERION_V2_SPATIAL_H

#include <rendering/Base.hpp>
#include <rendering/Material.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderBucket.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/IndirectDraw.hpp>
#include <animation/Skeleton.hpp>
#include <core/Scheduler.hpp>
#include <core/lib/FlatSet.hpp>

#include "Controller.hpp"

#include <rendering/backend/RendererStructs.hpp>

#include <math/Transform.hpp>
#include <math/BoundingBox.hpp>

#include <util/Defines.hpp>

#include <mutex>
#include <vector>

namespace hyperion::v2 {

using renderer::VertexAttributeSet;
using renderer::AccelerationStructure;
using renderer::AccelerationGeometry;
using renderer::FaceCullMode;

class RendererInstance;
class Octree;
class Scene;
class Entity;
class Mesh;

template<>
struct ComponentInitInfo<STUB_CLASS(Entity)>
{
    enum Flags : ComponentFlagBits
    {
        ENTITY_FLAGS_NONE              = 0x0,
        ENTITY_FLAGS_RAY_TESTS_ENABLED = 0x1
    };

    ComponentFlagBits flags = ENTITY_FLAGS_RAY_TESTS_ENABLED;
};

class Entity :
    public EngineComponentBase<STUB_CLASS(Entity)>,
    public HasDrawProxy<STUB_CLASS(Entity)>
{
    friend class Octree;
    friend class RendererInstance;
    friend class Controller;
    friend class Node;
    friend class Scene;

public:
    Entity();

    Entity(
        Handle<Mesh> &&mesh,
        Handle<Shader> &&shader,
        Handle<Material> &&material
    );

    Entity(
        Handle<Mesh> &&mesh,
        Handle<Shader> &&shader,
        Handle<Material> &&material,
        const RenderableAttributeSet &renderable_attributes,
        const ComponentInitInfo &init_info = {}
    );

    Entity(const Entity &other) = delete;
    Entity &operator=(const Entity &other) = delete;
    ~Entity();

    Octree *GetOctree() const
        { return m_octree; }

    ShaderDataState GetShaderDataState() const
        { return m_shader_data_state; }

    void SetShaderDataState(ShaderDataState state)
        { m_shader_data_state = state; }
    
    Handle<Mesh> &GetMesh()
        { return m_mesh; }

    const Handle<Mesh> &GetMesh() const
        { return m_mesh; }

    void SetMesh(Handle<Mesh> &&mesh);

    Handle<Skeleton> &GetSkeleton()
        { return m_skeleton; }

    const Handle<Skeleton> &GetSkeleton() const
        { return m_skeleton; }

    void SetSkeleton(Handle<Skeleton> &&skeleton);

    Handle<Shader> &GetShader()
        { return m_shader; }

    const Handle<Shader> &GetShader() const
        { return m_shader; }

    void SetShader(Handle<Shader> &&shader);

    Handle<Material> &GetMaterial()
        { return m_material; }

    const Handle<Material> &GetMaterial() const
        { return m_material; }

    void SetMaterial(Handle<Material> &&material);

    Node *GetParent() const
        { return m_node; }

    void SetParent(Node *node);

    Scene *GetScene() const
        { return m_scene; }

    // only call from Scene. Don't call manually.
    void SetScene(Scene *scene);

    bool IsRenderable() const
        { return m_mesh != nullptr && m_shader != nullptr && m_material != nullptr; }

    const RenderableAttributeSet &GetRenderableAttributes() const { return m_renderable_attributes; }

    void SetRenderableAttributes(const RenderableAttributeSet &renderable_attributes);

    // void SetMeshAttributes(
    //     VertexAttributeSet vertex_attributes,
    //     FaceCullMode face_cull_mode = FaceCullMode::BACK,
    //     bool depth_write = true,
    //     bool depth_test = true
    // );

    // void SetMeshAttributes(
    //     FaceCullMode face_cull_mode = FaceCullMode::BACK,
    //     bool depth_write = true,
    //     bool depth_test = true
    // );

    void SetStencilAttributes(const StencilState &stencil_state);

    RendererInstance *GetPrimaryRendererInstance() const
        { return m_primary_renderer_instance.renderer_instance; }

    Bucket GetBucket() const
        { return m_renderable_attributes.material_attributes.bucket; }

    void SetBucket(Bucket bucket);

    const Vector3 &GetTranslation() const
        { return m_transform.GetTranslation(); }

    void SetTranslation(const Vector3 &translation);

    const Vector3 &GetScale() const
        { return m_transform.GetScale(); }

    void SetScale(const Vector3 &scale);

    const Quaternion &GetRotation() const
        { return m_transform.GetRotation(); }

    void SetRotation(const Quaternion &rotation);

    const Transform &GetTransform() const
        { return m_transform; }

    void SetTransform(const Transform &transform);

    const BoundingBox &GetLocalAABB() const
        { return m_local_aabb; }

    void SetLocalAABB(const BoundingBox &aabb)
        { m_local_aabb = aabb; }

    const BoundingBox &GetWorldAABB() const
        { return m_world_aabb; }

    void SetWorldAABB(const BoundingBox &aabb)
        { m_world_aabb = aabb; }
    
    bool IsReady() const;

    void Init(Engine *engine);
    void Update(Engine *engine, GameCounter::TickUnit delta);

    template <class ControllerClass>
    void AddController(std::unique_ptr<ControllerClass> &&controller)
    {
        static_assert(std::is_base_of_v<Controller, ControllerClass>, "Object must be a derived class of Controller");

        if (controller->m_owner != nullptr) {
            controller->OnRemoved();
        }

        controller->m_owner = this;
        controller->OnAdded();
        controller->OnTransformUpdate(m_transform);

        m_controllers.Set(std::move(controller));
    }

    template <class ControllerClass, class ...Args>
    void AddController(Args &&... args)
        { AddController<ControllerClass>(std::make_unique<ControllerClass>(std::forward<Args>(args)...)); }

    template <class ControllerType>
    ControllerType *GetController()             { return m_controllers.Get<ControllerType>(); }

    template <class ControllerType>
    bool HasController() const                  { return m_controllers.Has<ControllerType>(); }

    template <class ControllerType>
    bool RemoveController()                     { return m_controllers.Remove<ControllerType>(); }
    
    ControllerSet &GetControllers()             { return m_controllers; }
    const ControllerSet &GetControllers() const { return m_controllers; }

public:
    void AddToOctree(Engine *engine, Octree &octree);

private:
    void RebuildRenderableAttributes();

    void UpdateControllers(Engine *engine, GameCounter::TickUnit delta);
    
    void EnqueueRenderUpdates();
    void UpdateOctree();
    
    void OnAddedToPipeline(RendererInstance *pipeline);
    void OnRemovedFromPipeline(RendererInstance *pipeline);
    
    void OnAddedToOctree(Octree *octree);
    void OnRemovedFromOctree(Octree *octree);
    void OnMovedToOctant(Octree *octree);

    void RemoveFromOctree(Engine *engine);

    Handle<Mesh> m_mesh;
    Handle<Shader> m_shader;
    Transform m_transform;
    BoundingBox m_local_aabb;
    BoundingBox m_world_aabb;
    Handle<Material> m_material;
    Handle<Skeleton> m_skeleton;
    Node *m_node;
    Scene *m_scene;

    RenderableAttributeSet m_renderable_attributes;

    ControllerSet m_controllers;

    std::atomic<Octree *> m_octree { nullptr };
    bool m_needs_octree_update { false };

    struct {
        RendererInstance *renderer_instance = nullptr;
        bool changed = false;
    } m_primary_renderer_instance;

    /* Retains a list of pointers to RendererInstances that this Entity is used by,
     * for easy removal when RemoveEntity() is called.
     */
    FlatSet<RendererInstance *> m_renderer_instances;
    std::mutex m_render_instances_mutex;

    mutable ShaderDataState m_shader_data_state;
};

} // namespace hyperion::v2

#endif
