#include "spatial.h"
#include <rendering/graphics.h>
#include <engine.h>

namespace hyperion::v2 {

Spatial::Spatial(
    Ref<Mesh> &&mesh,
    Ref<Shader> &&shader,
    Ref<Material> &&material,
    const RenderableAttributeSet &renderable_attributes
) : EngineComponentBase(),
    m_mesh(std::move(mesh)),
    m_shader(std::move(shader)),
    m_material(std::move(material)),
    m_renderable_attributes(renderable_attributes),
    m_octree(nullptr),
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

            if (m_primary_pipeline.pipeline == nullptr) {
                AddToPipeline(engine);
            }
        }

        if (m_octree == nullptr) {
            AddToOctree(engine);
        }

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SPATIALS, [this](Engine *engine) {
            RemoveFromPipelines(engine);

            if (m_octree != nullptr) {
                RemoveFromOctree(engine);
            }
            
            HYP_FLUSH_RENDER_QUEUE(engine);

            SetReady(false);
        }), engine);
    }));
}

void Spatial::Update(Engine *engine)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    if (m_skeleton != nullptr && m_skeleton->IsReady()) {
        m_skeleton->EnqueueRenderUpdates(engine);
    }

    if (m_material != nullptr && m_material->IsReady()) {
        m_material->Update(engine);
    }

    if (m_primary_pipeline.changed) {
        if (m_primary_pipeline.pipeline != nullptr) {
            RemoveFromPipeline(engine, m_primary_pipeline.pipeline);
        }

        AddToPipeline(engine);
    }

    if (m_shader_data_state.IsDirty()) {
        UpdateOctree(engine);

        EnqueueRenderUpdates(engine);
    }
}

void Spatial::EnqueueRenderUpdates(Engine *engine)
{
    AssertReady();

    const auto material_index = m_material != nullptr ? m_material->GetId().value - 1 : 0;

    engine->render_scheduler.EnqueueReplace(m_render_update_id, [this, engine, transform = m_transform, material_index](...) {
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

void Spatial::UpdateOctree(Engine *engine)
{
    if (Octree *octree = m_octree.load()) {
        if (!octree->Update(engine, this)) {
            DebugLog(
                LogType::Warn,
                "Could not update Spatial #%lu in octree\n",
                m_id.value
            );
        }
    }
}

void Spatial::SetMesh(Ref<Mesh> &&mesh)
{
    if (m_mesh == mesh) {
        return;
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

    m_material = std::move(material);

    if (m_material != nullptr && IsReady()) {
        m_material.Init();
    }

    m_shader_data_state |= ShaderDataState::DIRTY;
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

void Spatial::SetTransform(const Transform &transform)
{
    if (m_transform == transform) {
        return;
    }

    m_transform = transform;
    m_shader_data_state |= ShaderDataState::DIRTY;

    m_world_aabb = m_local_aabb * transform;
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

void Spatial::AddToPipeline(Engine *engine)
{
    if (const auto pipeline = engine->FindOrCreateGraphicsPipeline(m_renderable_attributes)) {
        AddToPipeline(engine, pipeline.ptr);
        
        m_primary_pipeline = {
            .pipeline = pipeline.ptr,
            .changed  = false
        };
    } else {
        DebugLog(
            LogType::Error,
            "Could not find or create optimal graphics pipeline for Spatial #%lu!\n",
            m_id.value
        );
    }
}

void Spatial::AddToPipeline(Engine *engine, GraphicsPipeline *pipeline)
{
    if (!m_pipelines.Contains(pipeline)) {
        pipeline->AddSpatial(this);
    }
}

void Spatial::RemoveFromPipelines(Engine *)
{
    auto pipelines = m_pipelines;

    for (auto *pipeline : pipelines) {
        if (pipeline == nullptr) {
            continue;
        }

        pipeline->OnSpatialRemoved(this);
    }

    m_pipelines.Clear();
    
    m_primary_pipeline = {
        .pipeline = nullptr,
        .changed  = true
    };
}

void Spatial::RemoveFromPipeline(Engine *, GraphicsPipeline *pipeline)
{
    if (pipeline == m_primary_pipeline.pipeline) {
        m_primary_pipeline = {
            .pipeline = nullptr,
            .changed  = true
        };
    }

    pipeline->OnSpatialRemoved(this);

    OnRemovedFromPipeline(pipeline);
}

void Spatial::OnAddedToOctree(Octree *octree)
{
    AssertThrow(m_octree == nullptr);
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Spatial #%lu added to octree\n", m_id.value);
#endif

    m_octree = octree;
    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Spatial::OnRemovedFromOctree(Octree *octree)
{
    AssertThrow(octree == m_octree);
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Spatial #%lu removed from octree\n", m_id.value);
#endif

    m_octree = nullptr;
    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Spatial::OnMovedToOctant(Octree *octree)
{
    AssertThrow(m_octree != nullptr);
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Spatial #%lu moved to new octant\n", m_id.value);
#endif

    m_octree = octree;
    m_shader_data_state |= ShaderDataState::DIRTY;
}


void Spatial::AddToOctree(Engine *engine)
{
    AssertThrow(m_octree == nullptr);

    if (!engine->GetOctree().Insert(engine, this)) {
        DebugLog(LogType::Warn, "Spatial #%lu could not be added to octree\n", m_id.value);
    }
}

void Spatial::RemoveFromOctree(Engine *engine)
{
    m_octree.load()->OnSpatialRemoved(engine, this);
}

bool Spatial::IsReady() const
{
    if (!Base::IsReady()) {
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
    
    return true;
}

} // namespace hyperion::v2
