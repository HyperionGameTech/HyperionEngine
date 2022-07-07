#include "Spatial.hpp"
#include <rendering/Graphics.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

Spatial::Spatial(
    Ref<Mesh> &&mesh,
    Ref<Shader> &&shader,
    Ref<Material> &&material,
    const RenderableAttributeSet &renderable_attributes,
    const ComponentInitInfo &init_info
) : EngineComponentBase(init_info),
    m_mesh(std::move(mesh)),
    m_shader(std::move(shader)),
    m_material(std::move(material)),
    m_node(nullptr),
    m_scene(nullptr),
    m_renderable_attributes(renderable_attributes),
    m_octree(nullptr),
    m_needs_octree_update(false),
    m_shader_data_state(ShaderDataState::DIRTY)
{
    if (m_mesh) {
        m_local_aabb = m_mesh->CalculateAabb();
        m_world_aabb = m_local_aabb * m_transform;
    }
}

Spatial::~Spatial()
{
    Teardown();
}

void Spatial::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SPATIALS, [this](Engine *engine) {
        if (m_material) {
            m_material.Init();
        }

        if (m_skeleton) {
            m_skeleton.Init();
        }

        if (m_mesh) {
            m_mesh.Init();
        }

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SPATIALS, [this](Engine *engine) {
            SetReady(false);

            m_material.Reset();

            engine->SafeReleaseRenderable(std::move(m_skeleton));
            //engine->SafeReleaseRenderable(std::move(m_material));
            engine->SafeReleaseRenderable(std::move(m_mesh));
            engine->SafeReleaseRenderable(std::move(m_shader));
        }), engine);
    }));
}

void Spatial::Update(Engine *engine, GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    if (m_skeleton != nullptr && m_skeleton->IsReady()) {
        m_skeleton->EnqueueRenderUpdates(engine);
    }

    if (m_material != nullptr && m_material->IsReady()) {
        m_material->Update(engine);
    }

    UpdateControllers(engine, delta);

    if (m_needs_octree_update) {
        UpdateOctree();
    }

    if (m_shader_data_state.IsDirty()) {
        EnqueueRenderUpdates(engine);
    }
}

void Spatial::UpdateControllers(Engine *engine, GameCounter::TickUnit delta)
{
    for (auto &it : m_controllers) {
        if (!it.second->ReceivesUpdate()) {
            continue;
        }

        it.second->OnUpdate(delta);
    }
}

void Spatial::EnqueueRenderUpdates(Engine *engine)
{
    AssertReady();

    const UInt32 material_index = m_material != nullptr
        ? m_material->GetId().value - 1
        : 0;

    engine->render_scheduler.Enqueue([this, engine, transform = m_transform, material_index](...) {
        engine->shader_globals->objects.Set(
            m_id.value - 1,
            {
                .model_matrix   = transform.GetMatrix(),
                .has_skinning   = m_skeleton != nullptr,
                .material_index = material_index,
                .local_aabb_max = Vector4(m_local_aabb.max, 1.0f),
                .local_aabb_min = Vector4(m_local_aabb.min, 1.0f),
                .world_aabb_max = Vector4(m_world_aabb.max, 1.0f),
                .world_aabb_min = Vector4(m_world_aabb.min, 1.0f)
            }
        );

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}

void Spatial::UpdateOctree()
{
    AssertThrow(IsInitCalled());

    if (Octree *octree = m_octree.load()) {
        const auto update_result = octree->Update(GetEngine(), this);

        if (!update_result) {
            DebugLog(
                LogType::Warn,
                "Could not update Spatial #%u in octree: %s\n",
                m_id.value,
                update_result.message
            );
        }
    }

    m_needs_octree_update = false;
}

void Spatial::SetMesh(Ref<Mesh> &&mesh)
{
    if (m_mesh == mesh) {
        return;
    }

    if (m_mesh != nullptr) {
        GetEngine()->SafeReleaseRenderable(std::move(m_mesh));
    }

    m_mesh = std::move(mesh);

    if (m_mesh != nullptr && IsReady()) {
        m_mesh.Init();
    }
}

void Spatial::SetSkeleton(Ref<Skeleton> &&skeleton)
{
    if (m_skeleton == skeleton) {
        return;
    }

    if (m_skeleton != nullptr) {
        GetEngine()->SafeReleaseRenderable(std::move(m_skeleton));
    }

    m_skeleton = std::move(skeleton);

    if (m_skeleton != nullptr && IsReady()) {
        m_skeleton.Init();
    }
}

void Spatial::SetShader(Ref<Shader> &&shader)
{
    if (m_shader == shader) {
        return;
    }

    if (m_shader != nullptr) {
        GetEngine()->SafeReleaseRenderable(std::move(m_shader));
    }

    m_shader = std::move(shader);
    
    RenderableAttributeSet new_renderable_attributes(m_renderable_attributes);
    new_renderable_attributes.shader_id = m_shader->GetId();

    SetRenderableAttributes(new_renderable_attributes);

    if (m_shader != nullptr && IsReady()) {
        m_shader.Init();
    }
}

void Spatial::SetMaterial(Ref<Material> &&material)
{
    if (m_material == material) {
        return;
    }

    //if (m_material != nullptr) {
    //    GetEngine()->SafeReleaseRenderable(std::move(m_material));
    //}

    m_material = std::move(material);

    if (m_material != nullptr && IsReady()) {
        m_material.Init();
    }

    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Spatial::SetParent(Node *node)
{
    if (m_node != nullptr) {
        for (auto &controller : m_controllers) {
            AssertThrow(controller.second != nullptr);

            controller.second->OnRemovedFromNode(m_node);
        }
    }

    m_node = node;

    if (m_node != nullptr) {
        for (auto &controller : m_controllers) {
            controller.second->OnAddedToNode(m_node);
        }
    }
}

void Spatial::SetRenderableAttributes(const RenderableAttributeSet &renderable_attributes)
{
    if (m_renderable_attributes == renderable_attributes) {
        return;
    }

    m_renderable_attributes    = renderable_attributes;
    m_primary_pipeline.changed = true;
}

void Spatial::SetMeshAttributes(
    VertexAttributeSet vertex_attributes,
    FaceCullMode face_cull_mode,
    bool depth_write,
    bool depth_test
)
{
    RenderableAttributeSet new_renderable_attributes(m_renderable_attributes);
    new_renderable_attributes.vertex_attributes = vertex_attributes;
    new_renderable_attributes.cull_faces        = face_cull_mode;
    new_renderable_attributes.depth_write       = depth_write;
    new_renderable_attributes.depth_test        = depth_test;

    SetRenderableAttributes(new_renderable_attributes);
}

void Spatial::SetMeshAttributes(
    FaceCullMode face_cull_mode,
    bool depth_write,
    bool depth_test
)
{
    SetMeshAttributes(
        m_renderable_attributes.vertex_attributes,
        face_cull_mode,
        depth_write,
        depth_test
    );
}

void Spatial::SetStencilAttributes(const StencilState &stencil_state)
{
    RenderableAttributeSet new_renderable_attributes(m_renderable_attributes);
    new_renderable_attributes.stencil_state = stencil_state;

    SetRenderableAttributes(new_renderable_attributes);
}

void Spatial::SetBucket(Bucket bucket)
{
    RenderableAttributeSet new_renderable_attributes(m_renderable_attributes);
    new_renderable_attributes.bucket = bucket;

    SetRenderableAttributes(new_renderable_attributes);
}

void Spatial::SetTranslation(const Vector3 &translation)
{
    if (m_node != nullptr) {
        // indirectly calls SetTransform() on this
        m_node->SetWorldTranslation(translation);
    } else {
        Transform new_transform(m_transform);
        new_transform.SetTranslation(translation);

        SetTransform(new_transform);
    }
}

void Spatial::SetScale(const Vector3 &scale)
{
    if (m_node != nullptr) {
        // indirectly calls SetTransform() on this
        m_node->SetWorldScale(scale);
    } else {
        Transform new_transform(m_transform);
        new_transform.SetScale(scale);

        SetTransform(new_transform);
    }
}

void Spatial::SetRotation(const Quaternion &rotation)
{
    if (m_node != nullptr) {
        // indirectly calls SetTransform() on this
        m_node->SetWorldRotation(rotation);
    } else {
        Transform new_transform(m_transform);
        new_transform.SetRotation(rotation);

        SetTransform(new_transform);
    }
}

void Spatial::SetTransform(const Transform &transform)
{
    if (m_transform == transform) {
        return;
    }

    m_transform = transform;
    m_shader_data_state |= ShaderDataState::DIRTY;

    m_world_aabb = m_local_aabb * transform;

    for (auto &it : m_controllers) {
        it.second->OnTransformUpdate(m_transform);
    }

    if (IsInitCalled()) {
        UpdateOctree();
    } else {
        m_needs_octree_update = true;
    }
}

void Spatial::OnAddedToPipeline(GraphicsPipeline *pipeline)
{
    m_pipelines.Insert(pipeline);
}

void Spatial::OnRemovedFromPipeline(GraphicsPipeline *pipeline)
{
    if (pipeline == m_primary_pipeline.pipeline) {
        m_primary_pipeline = {
            .pipeline = nullptr,
            .changed  = true
        };
    }

    m_pipelines.Erase(pipeline);
}

// void Spatial::RemoveFromPipelines()
// {
//     auto pipelines = m_pipelines;

//     for (auto *pipeline : pipelines) {
//         if (pipeline == nullptr) {
//             continue;
//         }

//         pipeline->OnSpatialRemoved(this);
//     }

//     m_pipelines.Clear();
    
//     m_primary_pipeline = {
//         .pipeline = nullptr,
//         .changed  = true
//     };
// }

// void Spatial::RemoveFromPipeline(Engine *, GraphicsPipeline *pipeline)
// {
//     if (pipeline == m_primary_pipeline.pipeline) {
//         m_primary_pipeline = {
//             .pipeline = nullptr,
//             .changed  = true
//         };
//     }

//     pipeline->OnSpatialRemoved(this);

//     OnRemovedFromPipeline(pipeline);
// }

void Spatial::OnAddedToOctree(Octree *octree)
{
    AssertThrow(m_octree == nullptr);

    if (m_id.value == 1) {
        DebugLog(LogType::Debug, "  1 ADDED\n");
    }
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Spatial #%lu added to octree\n", m_id.value);
#endif

    m_octree = octree;

    if (IsInitCalled()) {
        UpdateOctree();
    } else {
        m_needs_octree_update = true;
    }

    // m_shader_data_state |= ShaderDataState::DIRTY;
}

void Spatial::OnRemovedFromOctree(Octree *octree)
{
    AssertThrow(octree == m_octree);

    if (m_id.value == 1) {
        DebugLog(LogType::Debug, "  1 REMOVED\n");
    }
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Spatial #%lu removed from octree\n", m_id.value);
#endif

    m_octree = nullptr;
    // m_shader_data_state |= ShaderDataState::DIRTY;
}

void Spatial::OnMovedToOctant(Octree *octree)
{
    AssertThrow(m_octree != nullptr);

    DebugLog(LogType::Debug, "  %u MOVED\n", m_id.value);
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Spatial #%lu moved to new octant\n", m_id.value);
#endif

    m_octree = octree;

    if (IsInitCalled()) {
        UpdateOctree();
    } else {
        m_needs_octree_update = true;
    }

    // m_shader_data_state |= ShaderDataState::DIRTY;
}


void Spatial::AddToOctree(Engine *engine, Octree &octree)
{
    AssertThrow(m_octree == nullptr);

    if (!octree.Insert(engine, this)) {
        DebugLog(LogType::Warn, "Spatial #%lu could not be added to octree\n", m_id.value);
    }
}

void Spatial::RemoveFromOctree(Engine *engine)
{
    DebugLog(
        LogType::Debug,
        "Remove spatial #%u from octree\n",
        GetId().value
    );
    m_octree.load()->OnSpatialRemoved(engine, this);
}

bool Spatial::IsReady() const
{
    /*if (!Base::IsReady()) {
        return false;
    }
    
    if (m_skeleton != nullptr && !m_skeleton->IsReady()) {
        return false;
    }
    
    if (m_shader != nullptr && !m_shader->IsReady()) {
        return false;
    }
    
    if (m_mesh != nullptr && !m_mesh->IsReady()) {
        return false;
    }

    if (m_material != nullptr && !m_material->IsReady()) {
        return false;
    }
    
    return true;*/

    return Base::IsReady();
}

} // namespace hyperion::v2
