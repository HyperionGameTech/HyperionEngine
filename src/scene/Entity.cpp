#include "Entity.hpp"
#include <math/BoundingSphere.hpp>
#include <rendering/Renderer.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.hpp>

namespace hyperion::v2 {

Entity::Entity()
  : Entity(
        Handle<Mesh>(),
        Handle<Shader>(),
        Handle<Material>()
    )
{
}

Entity::Entity(
    Handle<Mesh> &&mesh,
    Handle<Shader> &&shader,
    Handle<Material> &&material
) : Entity(
        std::move(mesh),
        std::move(shader),
        std::move(material),
        {},
        {}
    )
{
}

Entity::Entity(
    Handle<Mesh> &&mesh,
    Handle<Shader> &&shader,
    Handle<Material> &&material,
    const RenderableAttributeSet &renderable_attributes,
    const InitInfo &init_info
) : EngineComponentBase(init_info),
    HasDrawProxy(),
    m_mesh(std::move(mesh)),
    m_shader(std::move(shader)),
    m_material(std::move(material)),
    m_node(nullptr),
    m_scene(nullptr),
    m_renderable_attributes(renderable_attributes),
    m_octree(nullptr),
    m_needs_octree_update(false),
    m_shader_data_state(ShaderDataState::CLEAN)
{
    if (m_mesh) {
        m_local_aabb = m_mesh->CalculateAABB();

        if (!m_local_aabb.Empty()) {
            m_world_aabb = m_local_aabb * m_transform;
        } else {
            m_world_aabb = BoundingBox::empty;
        }
    }
}

Entity::~Entity()
{
    Teardown();
}

void Entity::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    if (!m_shader) {
        m_shader = engine->shader_manager.GetShader(ShaderManager::Key::BASIC_FORWARD);
    }

    m_draw_proxy.entity_id = m_id;

    engine->InitObject(m_shader);
    engine->InitObject(m_material);
    engine->InitObject(m_skeleton);
    engine->InitObject(m_blas);

    // set our aabb to be the mesh aabb now that it is init
    if (engine->InitObject(m_mesh)) {
        m_local_aabb = m_mesh->CalculateAABB();

        if (!m_local_aabb.Empty()) {
            m_world_aabb = m_local_aabb * m_transform;
        } else {
            m_world_aabb = BoundingBox::empty;
        }

        m_needs_octree_update = true;
    }

    RebuildRenderableAttributes();

    SetReady(true);

#if defined(HYP_FEATURES_ENABLE_RAYTRACING) && HYP_FEATURES_ENABLE_RAYTRACING
    //if (engine->GetDevice()->GetFeatures().IsRaytracingEnabled()) {
    //    CreateBLAS();
    //}
#endif

    OnTeardown([this]() {
        auto *engine = GetEngine();

        DebugLog(
            LogType::Debug,
            "Destroy entity with id %u, with name %s\n",
            GetID().value,
            GetName().Data()
        );

        SetReady(false);

        // remove all controllers
        for (auto &it : m_controllers) {
            auto &controller = it.second;

            if (!controller) {
                continue;
            }

            if (m_node != nullptr) {
                controller->OnDetachedFromNode(m_node);
            }

            if (m_scene != nullptr) {
                controller->OnDetachedFromScene(m_scene);
            }

            controller->OnRemoved();
        }

        m_controllers.Clear();

        m_material.Reset();
        m_blas.Reset();

        WaitForRenderUpdatesToComplete();
        
        engine->SafeReleaseHandle<Skeleton>(std::move(m_skeleton));
        engine->SafeReleaseHandle<Mesh>(std::move(m_mesh));
        engine->SafeReleaseHandle<Shader>(std::move(m_shader));
    });
}

void Entity::Update(Engine *engine, GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    if (m_skeleton && m_skeleton->IsReady()) {
        m_skeleton->EnqueueRenderUpdates();
    }

    if (m_material != nullptr && m_material->IsReady()) {
        m_material->Update(engine);

        // make changes if it was updated
        if (m_material->GetRenderAttributes() != m_renderable_attributes.material_attributes) {
            m_renderable_attributes.material_attributes = m_material->GetRenderAttributes();
            SetRenderableAttributes(m_renderable_attributes);
        }
    }

    UpdateControllers(engine, delta);

    if (m_octree) {
        if (m_needs_octree_update) {
            UpdateOctree();
        }

        const auto &octree_visibility_state = m_octree->GetVisibilityState();
        const auto visibility_cursor = m_octree->LoadVisibilityCursor();

        m_visibility_state.snapshots[visibility_cursor] = octree_visibility_state.snapshots[visibility_cursor];
    }

    if (m_shader_data_state.IsDirty()) {
        EnqueueRenderUpdates();
    }
}

void Entity::UpdateControllers(Engine *engine, GameCounter::TickUnit delta)
{
    for (auto &it : m_controllers) {
        if (!it.second->ReceivesUpdate()) {
            continue;
        }

        it.second->OnUpdate(delta);
    }
}

void Entity::EnqueueRenderUpdates()
{
    AssertReady();

    const Skeleton::ID skeleton_id = m_skeleton != nullptr
        ? m_skeleton->GetID()
        : Skeleton::empty_id;

    const Material::ID material_id = m_material != nullptr
        ? m_material->GetID()
        : Material::empty_id;

    const Mesh::ID mesh_id = m_mesh != nullptr
        ? m_mesh->GetID()
        : Mesh::empty_id;

    const Scene::ID scene_id = m_scene != nullptr
        ? m_scene->GetID()
        : Scene::empty_id;

    const EntityDrawProxy draw_proxy {
        .mesh         = m_mesh.Get(),
        .material     = m_material.Get(),
        .entity_id    = m_id,
        .scene_id     = scene_id,
        .mesh_id      = mesh_id,
        .material_id  = material_id,
        .skeleton_id  = skeleton_id,
        .bounding_box = m_world_aabb,
        .bucket       = m_renderable_attributes.material_attributes.bucket
    };

    EnqueueRenderUpdate([this, transform_matrix = m_transform.GetMatrix(), draw_proxy](...) {
        // update m_draw_proxy on render thread.
        m_draw_proxy = draw_proxy;

        GetEngine()->GetRenderData()->objects.Set(
            m_id.value - 1,
            ObjectShaderData {
                .model_matrix = transform_matrix,
                .local_aabb_max = Vector4(m_local_aabb.max, 1.0f),
                .local_aabb_min = Vector4(m_local_aabb.min, 1.0f),
                .world_aabb_max = Vector4(m_world_aabb.max, 1.0f),
                .world_aabb_min = Vector4(m_world_aabb.min, 1.0f),
                .entity_id = draw_proxy.entity_id.value,
                .scene_id = draw_proxy.scene_id.value,
                .mesh_id = draw_proxy.mesh_id.value,
                .material_id = draw_proxy.material_id.value,
                .skeleton_id = draw_proxy.skeleton_id.value,
                .bucket = static_cast<UInt32>(draw_proxy.bucket)
            }
        );

        HYPERION_RETURN_OK;
    });

    // GetEngine()->render_scheduler.Enqueue([this, transform_matrix = m_transform.GetMatrix(), draw_proxy](...) {
    //     // update m_draw_proxy on render thread.
    //     m_draw_proxy = draw_proxy;

    //     GetEngine()->GetRenderData()->objects.Set(
    //         m_id.value - 1,
    //         ObjectShaderData {
    //             .model_matrix = transform_matrix,
    //             .local_aabb_max = Vector4(m_local_aabb.max, 1.0f),
    //             .local_aabb_min = Vector4(m_local_aabb.min, 1.0f),
    //             .world_aabb_max = Vector4(m_world_aabb.max, 1.0f),
    //             .world_aabb_min = Vector4(m_world_aabb.min, 1.0f),
    //             .entity_id = draw_proxy.entity_id.value,
    //             .scene_id = draw_proxy.scene_id.value,
    //             .mesh_id = draw_proxy.mesh_id.value,
    //             .material_id = draw_proxy.material_id.value,
    //             .skeleton_id = draw_proxy.skeleton_id.value,
    //             .bucket = static_cast<UInt32>(draw_proxy.bucket)
    //         }
    //     );

    //     HYPERION_RETURN_OK;
    // });

    m_shader_data_state = ShaderDataState::CLEAN;
}

void Entity::UpdateOctree()
{
    AssertReady();

    if (Octree *octree = m_octree) {
        const auto update_result = octree->Update(GetEngine(), this);

        if (!update_result) {
            DebugLog(
                LogType::Warn,
                "Could not update Entity #%u in octree: %s\n",
                m_id.value,
                update_result.message
            );
        }
    }

    m_needs_octree_update = false;
}

void Entity::SetMesh(Handle<Mesh> &&mesh)
{
    if (m_mesh == mesh) {
        return;
    }

    if (m_mesh && IsInitCalled()) {
        GetEngine()->SafeReleaseHandle<Mesh>(std::move(m_mesh));
    }

    m_mesh = std::move(mesh);

    if (IsInitCalled()) {
        if (m_mesh) {
            GetEngine()->InitObject(m_mesh);

            m_local_aabb = m_mesh->CalculateAABB();

            if (!m_local_aabb.Empty()) {
                m_world_aabb = m_local_aabb * m_transform;
            } else {
                m_world_aabb = BoundingBox::empty;
            }

            UpdateOctree();
        }
    }

    RebuildRenderableAttributes();
}

void Entity::SetSkeleton(Handle<Skeleton> &&skeleton)
{
    if (m_skeleton == skeleton) {
        return;
    }

    if (m_skeleton && IsInitCalled()) {
        GetEngine()->SafeReleaseHandle<Skeleton>(std::move(m_skeleton));
    }

    m_skeleton = std::move(skeleton);

    if (m_skeleton && IsInitCalled()) {
        GetEngine()->InitObject(m_skeleton);
    }

    RebuildRenderableAttributes();
}

void Entity::SetShader(Handle<Shader> &&shader)
{
    if (m_shader == shader) {
        return;
    }

    if (m_shader && IsInitCalled()) {
        GetEngine()->SafeReleaseHandle<Shader>(std::move(m_shader));
    }

    m_shader = std::move(shader);
    
    if (m_shader) {
        if (IsInitCalled()) {
            GetEngine()->InitObject(m_shader);
        }

        RenderableAttributeSet new_renderable_attributes(m_renderable_attributes);
        new_renderable_attributes.shader_id = m_shader->GetID();
        SetRenderableAttributes(new_renderable_attributes);
    } else {
        RebuildRenderableAttributes();
    }
}

void Entity::SetMaterial(Handle<Material> &&material)
{
    if (m_material == material) {
        return;
    }

    m_material = std::move(material);

    RenderableAttributeSet new_renderable_attributes(m_renderable_attributes);

    if (m_material) {
        new_renderable_attributes.material_attributes = m_material->GetRenderAttributes();

        if (IsInitCalled()) {
            GetEngine()->InitObject(m_material);
        }
    } else {
        new_renderable_attributes.material_attributes = { };
    }

    if (m_blas) {
        m_blas->SetMaterial(Handle<Material>(m_material));
    }

    SetRenderableAttributes(new_renderable_attributes);
}

void Entity::SetParent(Node *node)
{
    if (m_node != nullptr) {
        for (auto &controller : m_controllers) {
            AssertThrow(controller.second != nullptr);

            controller.second->OnDetachedFromNode(m_node);
        }
    }

    m_node = node;

    if (m_node != nullptr) {
        for (auto &controller : m_controllers) {
            controller.second->OnAttachedToNode(m_node);
        }
    }
}

void Entity::SetScene(Scene *scene)
{
    if (m_scene != nullptr) {
        if (auto &scene_tlas = m_scene->GetTLAS()) {
            if (m_blas) {
                //scene_tlas->RemoveBLAS(m_blas);
            }
        }

        for (auto &controller : m_controllers) {
            AssertThrow(controller.second != nullptr);

            controller.second->OnDetachedFromScene(m_scene);
        }
    }

    m_scene = scene;

    if (m_scene != nullptr) {
        if (auto &scene_tlas = m_scene->GetTLAS()) {
            if (m_blas) {
                scene_tlas->AddBLAS(Handle<BLAS>(m_blas));
            }
        }

        for (auto &controller : m_controllers) {
            controller.second->OnAttachedToScene(m_scene);
        }
    }

    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Entity::SetRenderableAttributes(const RenderableAttributeSet &renderable_attributes)
{
    // if (m_renderable_attributes == renderable_attributes) {
    //     return;
    // }

    m_renderable_attributes = renderable_attributes;
    m_primary_renderer_instance.changed = true;
    
    m_shader_data_state |= ShaderDataState::DIRTY;
}

void Entity::RebuildRenderableAttributes()
{
    RenderableAttributeSet new_renderable_attributes(
        m_mesh ? m_mesh->GetRenderAttributes() : MeshAttributes { .vertex_attributes = VertexAttributeSet(0u) },
        m_material ? m_material->GetRenderAttributes() : MaterialAttributes { }
    );

    if (m_skeleton) {
        new_renderable_attributes.mesh_attributes.vertex_attributes =
            new_renderable_attributes.mesh_attributes.vertex_attributes | renderer::skeleton_vertex_attributes;
    }

    new_renderable_attributes.shader_id = m_shader
        ? m_shader->GetID()
        : Shader::empty_id;

    SetRenderableAttributes(new_renderable_attributes);
}

void Entity::SetStencilAttributes(const StencilState &stencil_state)
{
    RenderableAttributeSet new_renderable_attributes(m_renderable_attributes);
    new_renderable_attributes.stencil_state = stencil_state;

    SetRenderableAttributes(new_renderable_attributes);
}

void Entity::SetTranslation(const Vector3 &translation)
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

void Entity::SetScale(const Vector3 &scale)
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

void Entity::SetRotation(const Quaternion &rotation)
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

void Entity::SetTransform(const Transform &transform)
{
    if (m_transform == transform) {
        return;
    }

    m_transform = transform;
    m_shader_data_state |= ShaderDataState::DIRTY;

    if (!m_local_aabb.Empty()) {
        m_world_aabb = m_local_aabb * transform;
    } else {
        m_world_aabb = BoundingBox::empty;
    }

    for (auto &it : m_controllers) {
        it.second->OnTransformUpdate(m_transform);
    }

    if (IsInitCalled()) {
        UpdateOctree();

        if (m_blas) {
            m_blas->SetTransform(m_transform);
        }
    } else {
        m_needs_octree_update = true;
    }
}

// TODO! Investigate if we even need those 2 functions
void Entity::OnAddedToPipeline(RendererInstance *pipeline)
{
    std::lock_guard guard(m_render_instances_mutex);

    m_renderer_instances.Insert(pipeline);
}

void Entity::OnRemovedFromPipeline(RendererInstance *pipeline)
{
    std::lock_guard guard(m_render_instances_mutex);

    if (pipeline == m_primary_renderer_instance.renderer_instance) {
        m_primary_renderer_instance = {
            .renderer_instance = nullptr,
            .changed  = true
        };
    }

    m_renderer_instances.Erase(pipeline);
}

void Entity::OnAddedToOctree(Octree *octree)
{
    AssertThrow(m_octree == nullptr);
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Entity #%lu added to octree\n", m_id.value);
#endif

    m_octree = octree;

    if (IsInitCalled()) {
        UpdateOctree();
    } else {
        m_needs_octree_update = true;
    }
}

void Entity::OnRemovedFromOctree(Octree *octree)
{
    AssertThrow(octree == m_octree);
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Entity #%lu removed from octree\n", m_id.value);
#endif

    m_octree = nullptr;
}

void Entity::OnMovedToOctant(Octree *octree)
{
    AssertThrow(m_octree != nullptr);
    
#if HYP_OCTREE_DEBUG
    DebugLog(LogType::Info, "Entity #%lu moved to new octant\n", m_id.value);
#endif

    m_octree = octree;

    if (IsInitCalled()) {
        UpdateOctree();
    } else {
        m_needs_octree_update = true;
    }
}


void Entity::AddToOctree(Engine *engine, Octree &octree)
{
    AssertThrow(m_octree == nullptr);

    if (!octree.Insert(engine, this)) {
        DebugLog(LogType::Warn, "Entity #%lu could not be added to octree\n", m_id.value);
    }
}

void Entity::RemoveFromOctree(Engine *engine)
{
    DebugLog(
        LogType::Debug,
        "Remove entity #%u from octree\n",
        GetID().value
    );

    m_octree->OnEntityRemoved(engine, this);
}

bool Entity::IsReady() const
{
    return Base::IsReady();
}

bool Entity::CreateBLAS()
{
    AssertReady();

    if (m_blas) {
        return true;
    }

#if defined(HYP_FEATURES_ENABLE_RAYTRACING) && HYP_FEATURES_ENABLE_RAYTRACING
    if (!GetEngine()->GetDevice()->GetFeatures().IsRaytracingEnabled()) {
        return false;
    }
#else
    return false;
#endif

    if (!IsRenderable()) {
        // needs mesh, material, etc.
        return false;
    }

    m_blas = GetEngine()->CreateHandle<BLAS>(
        m_id,
        Handle<Mesh>(m_mesh),
        Handle<Material>(m_material),
        m_transform
    );

    if (IsInitCalled()) {
        if (GetEngine()->InitObject(m_blas)) {
            // add it to the scene if it exists
            // otherwise, have to add to scene when the Entity is added to a scene

            if (m_scene && m_scene->GetTLAS()) {
                m_scene->GetTLAS()->AddBLAS(Handle<BLAS>(m_blas));
            }

            return true;
        }

        m_blas.Reset();

        return false;
    } else {
        return true;
    }
}

} // namespace hyperion::v2
