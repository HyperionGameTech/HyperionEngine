#ifndef HYPERION_V2_SPATIAL_H
#define HYPERION_V2_SPATIAL_H

#include <rendering/Base.hpp>
#include <rendering/Material.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderBucket.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <animation/Skeleton.hpp>
#include <core/Scheduler.hpp>
#include <core/lib/FlatSet.hpp>

#include "Controller.hpp"

#include <rendering/backend/RendererStructs.hpp>

#include <math/Transform.hpp>
#include <math/BoundingBox.hpp>

#include <util/Defines.hpp>

#include <vector>

namespace hyperion::v2 {

using renderer::VertexAttributeSet;
using renderer::AccelerationStructure;
using renderer::AccelerationGeometry;
using renderer::FaceCullMode;

class GraphicsPipeline;
class Octree;
class Scene;

template<>
struct ComponentInitInfo<STUB_CLASS(Spatial)> {
    enum Flags : ComponentFlagBits {
        ENTITY_FLAGS_NONE              = 0x0,
        ENTITY_FLAGS_RAY_TESTS_ENABLED = 0x1
    };

    ComponentFlagBits flags = ENTITY_FLAGS_RAY_TESTS_ENABLED;
};

class Spatial : public EngineComponentBase<STUB_CLASS(Spatial)> {
    friend class Octree;
    friend class GraphicsPipeline;
    friend class Controller;
    friend class Node;
    friend class Scene;

public:
    Spatial(
        Ref<Mesh> &&mesh                                    = nullptr,
        Ref<Shader> &&shader                                = nullptr,
        Ref<Material> &&material                            = nullptr,
        const RenderableAttributeSet &renderable_attributes = {},
        const ComponentInitInfo &init_info                  = {}
    );

    Spatial(const Spatial &other) = delete;
    Spatial &operator=(const Spatial &other) = delete;
    ~Spatial();

    Octree *GetOctree() const { return m_octree; }

    ShaderDataState GetShaderDataState() const     { return m_shader_data_state; }
    void SetShaderDataState(ShaderDataState state) { m_shader_data_state = state; }
    
    Ref<Mesh> &GetMesh()                     { return m_mesh; }
    const Ref<Mesh> &GetMesh() const         { return m_mesh; }
    void SetMesh(Ref<Mesh> &&mesh);

    Ref<Skeleton> &GetSkeleton()             { return m_skeleton; }
    const Ref<Skeleton> &GetSkeleton() const { return m_skeleton; }
    void SetSkeleton(Ref<Skeleton> &&skeleton);

    Ref<Shader> &GetShader()                 { return m_shader; }
    const Ref<Shader> &GetShader() const     { return m_shader; }
    void SetShader(Ref<Shader> &&shader);

    Ref<Material> &GetMaterial()             { return m_material; }
    const Ref<Material> &GetMaterial() const { return m_material; }
    void SetMaterial(Ref<Material> &&material);

    Node *GetParent() const                  { return m_node; }
    void SetParent(Node *node);

    Scene *GetScene() const                  { return m_scene; }

    bool IsRenderable() const                { return m_mesh != nullptr && m_shader != nullptr && m_material != nullptr; }

    const RenderableAttributeSet &GetRenderableAttributes() const { return m_renderable_attributes; }
    void SetRenderableAttributes(const RenderableAttributeSet &render_options);

    void SetMeshAttributes(
        VertexAttributeSet vertex_attributes,
        FaceCullMode face_cull_mode = FaceCullMode::BACK,
        bool depth_write            = true,
        bool depth_test             = true
    );

    void SetMeshAttributes(
        FaceCullMode face_cull_mode = FaceCullMode::BACK,
        bool depth_write            = true,
        bool depth_test             = true
    );

    void SetStencilAttributes(const StencilState &stencil_state);

    GraphicsPipeline *GetPrimaryPipeline() const { return m_primary_pipeline.pipeline; }

    // auto &GetPipelines()                   { return m_pipelines; }
    const auto &GetPipelines() const        { return m_pipelines; }

    Bucket GetBucket() const                { return m_renderable_attributes.bucket; }
    void SetBucket(Bucket bucket);

    const Vector3 &GetTranslation() const   { return m_transform.GetTranslation(); }
    void SetTranslation(const Vector3 &translation);

    const Vector3 &GetScale() const         { return m_transform.GetScale(); }
    void SetScale(const Vector3 &scale);

    const Quaternion &GetRotation() const   { return m_transform.GetRotation(); }
    void SetRotation(const Quaternion &rotation);

    const Transform &GetTransform() const   { return m_transform; }
    void SetTransform(const Transform &transform);

    const BoundingBox &GetLocalAabb() const { return m_local_aabb; }
    const BoundingBox &GetWorldAabb() const { return m_world_aabb; }
    
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
    void UpdateControllers(Engine *engine, GameCounter::TickUnit delta);
    
    void EnqueueRenderUpdates(Engine *engine);
    void UpdateOctree();
    
    void OnAddedToPipeline(GraphicsPipeline *pipeline);
    void OnRemovedFromPipeline(GraphicsPipeline *pipeline);
    
    // void RemoveFromPipelines();
    // void RemoveFromPipeline(Engine *engine, GraphicsPipeline *pipeline);
    
    void OnAddedToOctree(Octree *octree);
    void OnRemovedFromOctree(Octree *octree);
    void OnMovedToOctant(Octree *octree);

    void RemoveFromOctree(Engine *engine);

    Ref<Mesh>              m_mesh;
    Ref<Shader>            m_shader;
    Transform              m_transform;
    BoundingBox            m_local_aabb;
    BoundingBox            m_world_aabb;
    Ref<Material>          m_material;
    Ref<Skeleton>          m_skeleton;
    Node                  *m_node;
    Scene                 *m_scene;
    RenderableAttributeSet m_renderable_attributes;

    ControllerSet          m_controllers;

    std::atomic<Octree *>  m_octree{nullptr};
    // std::atomic_bool       m_is_visible{false};
    bool                   m_needs_octree_update{false};

    struct {
        GraphicsPipeline *pipeline = nullptr;
        bool changed               = false;
    } m_primary_pipeline;

    /* Retains a list of pointers to pipelines that this Spatial is used by,
     * for easy removal when RemoveSpatial() is called.
     */
    FlatSet<GraphicsPipeline *> m_pipelines;

    mutable ShaderDataState     m_shader_data_state;
    ScheduledFunctionId         m_render_update_id,
                                m_change_pipeline_id;
};

} // namespace hyperion::v2

#endif
