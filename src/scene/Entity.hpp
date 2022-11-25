#ifndef HYPERION_V2_SPATIAL_H
#define HYPERION_V2_SPATIAL_H

#include <core/Base.hpp>
#include <rendering/Material.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderBucket.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/rt/BLAS.hpp>
#include <scene/animation/Skeleton.hpp>
#include <scene/VisibilityState.hpp>
#include <scene/Controller.hpp>
#include <math/Transform.hpp>
#include <math/BoundingBox.hpp>
#include <core/Scheduler.hpp>
#include <core/lib/FlatSet.hpp>

#include <rendering/backend/RendererStructs.hpp>

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
        ENTITY_FLAGS_NONE = 0x0,
        ENTITY_FLAGS_RAY_TESTS_ENABLED = 0x1
    };

    ComponentFlagBits flags = ENTITY_FLAGS_RAY_TESTS_ENABLED;
};

class Entity :
    public EngineComponentBase<STUB_CLASS(Entity)>,
    public HasDrawProxy<STUB_CLASS(Entity)>
{
    friend struct RenderCommand_UpdateEntityRenderData;

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
        const InitInfo &init_info = { }
    );

    Entity(const Entity &other) = delete;
    Entity &operator=(const Entity &other) = delete;
    ~Entity();

    const VisibilityState &GetVisibilityState() const
        { return m_visibility_state; }

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

    Handle<BLAS> &GetBLAS()
        { return m_blas; }

    const Handle<BLAS> &GetBLAS() const
        { return m_blas; }
    
    /*! \brief Creates a bottom level acceleration structure for this Entity. If one already exists on this Entity,
     *  no action is performed and true is returned. If the BLAS could not be created, false is returned.
     *  The Entity must be in a READY state to call this.
     */
    bool CreateBLAS();

    Node *GetParent() const
        { return m_node; }

    void SetParent(Node *node);

    Scene *GetScene() const
        { return m_scene; }

    // only call from Scene. Don't call manually.
    void SetScene(Scene *scene);

    bool IsRenderable() const
        { return m_mesh && m_shader && m_material; }

    const RenderableAttributeSet &GetRenderableAttributes() const { return m_renderable_attributes; }
    void SetRenderableAttributes(const RenderableAttributeSet &renderable_attributes);
    void RebuildRenderableAttributes();

    void SetStencilAttributes(const StencilState &stencil_state);

    RendererInstance *GetPrimaryRendererInstance() const
        { return m_primary_renderer_instance.renderer_instance; }

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

    void Init();
    void Update( GameCounter::TickUnit delta);

    /* All controller operations should only be used from the GAME thread */

    template <class ControllerClass>
    void AddController(UniquePtr<ControllerClass> &&controller)
    {
        static_assert(std::is_base_of_v<Controller, ControllerClass>, "Object must be a derived class of Controller");

        if (controller->m_owner != nullptr) {
            // Controller already has a parent. Remove it from the current parent.
            controller->m_owner->template RemoveController<ControllerClass>();
        }

        controller->m_owner = this;
        controller->OnAdded();

        if (m_node != nullptr) {
            controller->OnAttachedToNode(m_node);
        }

        if (m_scene != nullptr) {
            controller->OnAttachedToScene(m_scene);
        }

        controller->OnTransformUpdate(m_transform);

        m_controllers.Set(std::move(controller));
    }

    template <class ControllerClass, class ...Args>
    void AddController(Args &&... args)
        { AddController<ControllerClass>(UniquePtr<ControllerClass>::Construct(std::forward<Args>(args)...)); }

    template <class ControllerClass>
    bool RemoveController()
    {
        if (auto *controller = m_controllers.Get<ControllerClass>()) {
            if (m_node != nullptr) {
                controller->OnDetachedFromNode(m_node);
            }

            if (m_scene != nullptr) {
                controller->OnDetachedFromScene(m_scene);
            }

            controller->OnRemoved();

            return m_controllers.Remove<ControllerClass>();
        } else {
            return false;
        }
    }

    template <class ControllerType>
    ControllerType *GetController()
        { return m_controllers.Get<ControllerType>(); }

    template <class ControllerType>
    bool HasController() const
        { return m_controllers.Has<ControllerType>(); }
    
    ControllerSet &GetControllers()
        { return m_controllers; }

    const ControllerSet &GetControllers() const
        { return m_controllers; }

public:
    void AddToOctree( Octree &octree);

private:
    void UpdateControllers( GameCounter::TickUnit delta);
    
    void EnqueueRenderUpdates();
    void UpdateOctree();
    
    void OnAddedToPipeline(RendererInstance *pipeline);
    void OnRemovedFromPipeline(RendererInstance *pipeline);
    
    void OnAddedToOctree(Octree *octree);
    void OnRemovedFromOctree(Octree *octree);
    void OnMovedToOctant(Octree *octree);

    void RemoveFromOctree();

    Handle<Mesh> m_mesh;
    Handle<Shader> m_shader;
    Transform m_transform;
    BoundingBox m_local_aabb;
    BoundingBox m_world_aabb;
    Handle<Material> m_material;
    Handle<Skeleton> m_skeleton;
    Handle<BLAS> m_blas;

public: // temp
    Node *m_node;
    Scene *m_scene;

private:

    RenderableAttributeSet m_renderable_attributes;

    ControllerSet m_controllers;

    Octree *m_octree = nullptr;
    bool m_needs_octree_update = false;
    VisibilityState m_visibility_state;

    struct {
        RendererInstance *renderer_instance = nullptr;
        bool changed = false;
    } m_primary_renderer_instance;

    /* Retains a list of pointers to RendererInstances that this Entity is used by,
     * for easy removal when RemoveEntity() is called.
     */
    FlatSet<RendererInstance *> m_renderer_instances;
    std::mutex m_render_instances_mutex;

    Matrix4 m_previous_transform_matrix;

    mutable ShaderDataState m_shader_data_state;
};

} // namespace hyperion::v2

#endif
