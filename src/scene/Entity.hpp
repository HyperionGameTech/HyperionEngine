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

class RenderGroup;
class Octree;
class Scene;
class Camera;
class Entity;
class Mesh;

template<>
struct ComponentInitInfo<STUB_CLASS(Entity)>
{
    enum Flags : ComponentFlags
    {
        ENTITY_FLAGS_NONE = 0x0,
        ENTITY_FLAGS_RAY_TESTS_ENABLED = 0x1,
        ENTITY_FLAGS_HAS_BLAS = 0x2,
        ENTITY_FLAGS_INCLUDE_IN_INDIRECT_LIGHTING = 0x4
    };

    ComponentFlags flags = ENTITY_FLAGS_RAY_TESTS_ENABLED
        | ENTITY_FLAGS_HAS_BLAS
        | ENTITY_FLAGS_INCLUDE_IN_INDIRECT_LIGHTING;
};

class Entity :
    public BasicObject<STUB_CLASS(Entity)>,
    public HasDrawProxy<STUB_CLASS(Entity)>
{
    friend struct RenderCommand_UpdateEntityRenderData;

    friend class Octree;
    friend class RenderGroup;
    friend class Controller;
    friend class Node;
    friend class Scene;

public:
    Entity();

    Entity(Name);

    Entity(
        Name,
        Handle<Mesh> mesh,
        Handle<Shader> shader,
        Handle<Material> material
    );

    Entity(
        Handle<Mesh> mesh,
        Handle<Shader> shader,
        Handle<Material> material
    );

    Entity(
        Name,
        Handle<Mesh> mesh,
        Handle<Shader> shader,
        Handle<Material> material,
        const RenderableAttributeSet &renderable_attributes,
        const InitInfo &init_info = {
            InitInfo::Flags::ENTITY_FLAGS_RAY_TESTS_ENABLED
            | InitInfo::Flags::ENTITY_FLAGS_HAS_BLAS
            | InitInfo::Flags::ENTITY_FLAGS_INCLUDE_IN_INDIRECT_LIGHTING
        }
    );

    Entity(
        Handle<Mesh> mesh,
        Handle<Shader> shader,
        Handle<Material> material,
        const RenderableAttributeSet &renderable_attributes,
        const InitInfo &init_info = {
            InitInfo::Flags::ENTITY_FLAGS_RAY_TESTS_ENABLED
            | InitInfo::Flags::ENTITY_FLAGS_HAS_BLAS
            | InitInfo::Flags::ENTITY_FLAGS_INCLUDE_IN_INDIRECT_LIGHTING
        }
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
    void SetMesh(const Handle<Mesh> &mesh)
        { SetMesh(Handle<Mesh>(mesh)); }

    Handle<Skeleton> &GetSkeleton()
        { return m_skeleton; }

    const Handle<Skeleton> &GetSkeleton() const
        { return m_skeleton; }

    void SetSkeleton(Handle<Skeleton> &&skeleton);
    void SetSkeleton(const Handle<Skeleton> &skeleton)
        { SetSkeleton(Handle<Skeleton>(skeleton)); }

    Handle<Shader> &GetShader()
        { return m_shader; }

    const Handle<Shader> &GetShader() const
        { return m_shader; }

    void SetShader(Handle<Shader> &&shader);
    void SetShader(const Handle<Shader> &shader)
        { SetShader(Handle<Shader>(shader)); }

    Handle<Material> &GetMaterial()
        { return m_material; }

    const Handle<Material> &GetMaterial() const
        { return m_material; }

    void SetMaterial(Handle<Material> &&material);
    void SetMaterial(const Handle<Material> &material)
        { SetMaterial(Handle<Material>(material)); }

    Handle<BLAS> &GetBLAS()
        { return m_blas; }

    const Handle<BLAS> &GetBLAS() const
        { return m_blas; }
    
    /*! \brief Creates a bottom level acceleration structure for this Entity. If one already exists on this Entity,
     *  no action is performed and true is returned. If the BLAS could not be created, false is returned.
     *  The Entity must be in a READY state to call this.
     */
    Bool CreateBLAS();

    void SetIsAttachedToNode(Node *node, Bool is_attached_to_node);

    Bool IsInScene(ID<Scene> id) const
        { return m_scenes.Contains(id); }

    // only call from Scene. Don't call manually.
    void SetIsInScene(ID<Scene> id, Bool is_in_scene);

    const FlatSet<ID<Scene>> &GetScenes() const
        { return m_scenes; }

    const Array<Node *> &GetNodes() const
        { return m_nodes; }

    Bool IsRenderable() const
        { return m_mesh && m_shader && m_material; }

    const RenderableAttributeSet &GetRenderableAttributes() const { return m_renderable_attributes; }
    void SetRenderableAttributes(const RenderableAttributeSet &renderable_attributes);
    void RebuildRenderableAttributes();

    void SetStencilAttributes(const StencilState &stencil_state);

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

    const Matrix4 &GetPreviousModelMatrix() const
        { return m_previous_model_matrix; }

    const BoundingBox &GetLocalAABB() const
        { return m_local_aabb; }

    void SetLocalAABB(const BoundingBox &aabb)
        { m_local_aabb = aabb; UpdateWorldAABB(); }

    const BoundingBox &GetWorldAABB() const
        { return m_world_aabb; }

    void SetWorldAABB(const BoundingBox &aabb)
        { m_world_aabb = aabb; }

    Bool IsVisibleToCamera(ID<Camera> camera_id, UInt8 visibility_cursor) const;
    
    Bool IsReady() const;

    void Init();
    void Update(GameCounter::TickUnit delta);

    //// Deprecated: Use new ECS system

    /* All controller operations should only be used from the GAME thread */

    Controller *AddController(TypeID type_id, UniquePtr<Controller> &&controller)
    {
        AssertThrow(controller != nullptr);

        if (controller->m_owner != nullptr) {
            // Controller already has a parent. Remove it from the current parent.
            controller->m_owner->RemoveController(type_id);
        }

        controller->m_owner = this;
        controller->OnAdded();

        for (Node *node : m_nodes) {
            controller->OnAttachedToNode(node);
        }

        for (const ID<Scene> &id : m_scenes) {
            controller->OnAttachedToScene(id);
        }

        controller->OnTransformUpdate(m_transform);

        Controller *ptr = controller.Get();
        m_controllers.Set(type_id, std::move(controller));

        return ptr;
    }

    template <class ControllerClass>
    ControllerClass *AddController(UniquePtr<ControllerClass> &&controller)
    {
        AssertThrow(controller != nullptr);

        static_assert(std::is_base_of_v<Controller, ControllerClass>, "Object must be a derived class of Controller");

        if (controller->m_owner != nullptr) {
            // Controller already has a parent. Remove it from the current parent.
            controller->m_owner->template RemoveController<ControllerClass>();
        }

        controller->m_owner = this;
        controller->OnAdded();

        for (Node *node : m_nodes) {
            controller->OnAttachedToNode(node);
        }

        for (const ID<Scene> &id : m_scenes) {
            controller->OnAttachedToScene(id);
        }

        controller->OnTransformUpdate(m_transform);

        ControllerClass *ptr = controller.Get();
        m_controllers.Set(std::move(controller));

        return ptr;
    }

    template <class ControllerClass, class ...Args>
    ControllerClass *AddController(Args &&... args)
        { return AddController<ControllerClass>(UniquePtr<ControllerClass>::Construct(std::forward<Args>(args)...)); }

    Bool RemoveController(TypeID type_id)
    {
        if (auto *controller = m_controllers.Get(type_id)) {
            for (Node *node : m_nodes) {
                controller->OnDetachedFromNode(node);
            }

            for (const ID<Scene> &id : m_scenes) {
                controller->OnDetachedFromScene(id);
            }

            controller->OnRemoved();

            return m_controllers.Remove(type_id);
        } else {
            return false;
        }
    }

    template <class ControllerClass>
    Bool RemoveController()
    {
        if (auto *controller = m_controllers.Get<ControllerClass>()) {
            for (Node *node : m_nodes) {
                controller->OnDetachedFromNode(node);
            }

            for (const ID<Scene> &id : m_scenes) {
                controller->OnDetachedFromScene(id);
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
    Bool HasController() const
        { return m_controllers.Has<ControllerType>(); }
    
    ControllerSet &GetControllers()
        { return m_controllers; }

    const ControllerSet &GetControllers() const
        { return m_controllers; }

    //// End deprecated
    
    void AddToOctree(Octree &octree);
    void SetNeedsOctreeUpdate()
        { m_needs_octree_update = true; }

private:
    void UpdateWorldAABB(Bool propagate_to_controllers = true);

    void UpdateControllers(GameCounter::TickUnit delta);
    
    void EnqueueRenderUpdates();
    void UpdateOctree();
    
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
    Array<Node *> m_nodes;

private:
    
    FlatSet<ID<Scene>>      m_scenes;

    RenderableAttributeSet  m_renderable_attributes;

    ControllerSet           m_controllers;

    Octree                  *m_octree = nullptr;
    Bool                    m_needs_octree_update = false;
    VisibilityState         m_visibility_state;

    Matrix4                 m_previous_model_matrix;

    mutable ShaderDataState m_shader_data_state;
};

} // namespace hyperion::v2

#endif
