#include "Entity.hpp"
#include <math/BoundingSphere.hpp>
#include <rendering/Renderer.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/RenderCommands.hpp>

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.hpp>

namespace hyperion::v2 {

class Entity;

struct RENDER_COMMAND(UpdateEntityRenderData) : RenderCommand
{
    Entity *entity;
    EntityDrawProxy draw_proxy;
    Matrix4 transform_matrix;
    Matrix4 previous_transform_matrix;

    RenderCommand_UpdateEntityRenderData(
        Entity *entity,
        EntityDrawProxy &&draw_proxy,
        const Matrix4 &transform_matrix,
        const Matrix4 &previous_transform_matrix
    ) : entity(entity),
        draw_proxy(std::move(draw_proxy)),
        transform_matrix(transform_matrix),
        previous_transform_matrix(previous_transform_matrix)
    {
    }

    virtual Result operator()()
    {
        entity->m_draw_proxy = draw_proxy;

        Engine::Get()->GetRenderData()->objects.Set(
            entity->GetID().ToIndex(),
            ObjectShaderData {
                .model_matrix = transform_matrix,
                .previous_model_matrix = previous_transform_matrix,
                .world_aabb_max = Vector4(draw_proxy.bounding_box.max, 1.0f),
                .world_aabb_min = Vector4(draw_proxy.bounding_box.min, 1.0f),
                .entity_index = draw_proxy.entity_id.ToIndex(),
                .scene_index = draw_proxy.scene_id.ToIndex(),
                .material_index = draw_proxy.material_id.ToIndex(),
                .skeleton_index = draw_proxy.skeleton_id.ToIndex(),
                .bucket = UInt32(draw_proxy.bucket),
                .flags = draw_proxy.skeleton_id ? ENTITY_GPU_FLAG_HAS_SKELETON : ENTITY_GPU_FLAG_NONE
            }
        );
    
        HYPERION_RETURN_OK;
    }
};

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
    m_shader_data_state(ShaderDataState::DIRTY)
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

void Entity::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    if (!m_shader) {
        m_shader = Engine::Get()->shader_manager.GetShader(ShaderManager::Key::BASIC_FORWARD);
    }

    m_draw_proxy.entity_id = m_id;
    m_previous_transform_matrix = m_transform.GetMatrix();

    InitObject(m_shader);
    InitObject(m_material);
    InitObject(m_skeleton);
    InitObject(m_blas);

    // set our aabb to be the mesh aabb now that it is init
    if (InitObject(m_mesh)) {
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
    if (Engine::Get()->GetGPUDevice()->GetFeatures().IsRaytracingEnabled() && HasFlags(InitInfo::ENTITY_FLAGS_HAS_BLAS)) {
       CreateBLAS();
    }
#endif

    OnTeardown([this]() {
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

        HYP_SYNC_RENDER();
        
        Engine::Get()->SafeReleaseHandle<Skeleton>(std::move(m_skeleton));
        Engine::Get()->SafeReleaseHandle<Mesh>(std::move(m_mesh));
        Engine::Get()->SafeReleaseHandle<Shader>(std::move(m_shader));
    });
}

void Entity::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    if (m_skeleton && m_skeleton->IsReady()) {
        m_skeleton->EnqueueRenderUpdates();
    }

    if (m_material != nullptr && m_material->IsReady()) {
        m_material->Update();

        // make changes if it was updated
        if (m_material->GetRenderAttributes() != m_renderable_attributes.material_attributes) {
            m_renderable_attributes.material_attributes = m_material->GetRenderAttributes();
            SetRenderableAttributes(m_renderable_attributes);
        }
    }

    UpdateControllers(delta);

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

void Entity::UpdateControllers(GameCounter::TickUnit delta)
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

    const ID<Skeleton> skeleton_id = m_skeleton
        ? m_skeleton->GetID()
        : Skeleton::empty_id;

    const ID<Material> material_id = m_material
        ? m_material->GetID()
        : Material::empty_id;

    const ID<Mesh> mesh_id = m_mesh
        ? m_mesh->GetID()
        : Mesh::empty_id;

    const ID<Scene> scene_id = m_scene
        ? m_scene->GetID()
        : Scene::empty_id;

    EntityDrawProxy draw_proxy {
        .mesh = m_mesh.Get(),
        .material = m_material.Get(),
        .entity_id = m_id,
        .scene_id = scene_id,
        .mesh_id = mesh_id,
        .material_id = material_id,
        .skeleton_id = skeleton_id,
        .bounding_box = m_world_aabb,
        .bucket = m_renderable_attributes.material_attributes.bucket
    };

    RenderCommands::Push<RENDER_COMMAND(UpdateEntityRenderData)>(
        this,
        std::move(draw_proxy),
        m_transform.GetMatrix(),
        m_previous_transform_matrix
    );

    if (m_previous_transform_matrix == m_transform.GetMatrix()) {
        m_shader_data_state = ShaderDataState::CLEAN;
    }

    m_previous_transform_matrix = m_transform.GetMatrix();
}

void Entity::UpdateOctree()
{
    AssertReady();

    if (Octree *octree = m_octree) {
        const auto update_result = octree->Update(this);

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
        Engine::Get()->SafeReleaseHandle<Mesh>(std::move(m_mesh));
    }

    m_mesh = std::move(mesh);

    if (IsInitCalled()) {
        if (InitObject(m_mesh)) {
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
        Engine::Get()->SafeReleaseHandle<Skeleton>(std::move(m_skeleton));
    }

    m_skeleton = std::move(skeleton);

    if (m_skeleton && IsInitCalled()) {
        InitObject(m_skeleton);
    }

    RebuildRenderableAttributes();
}

void Entity::SetShader(Handle<Shader> &&shader)
{
    if (m_shader == shader) {
        return;
    }

    if (m_shader && IsInitCalled()) {
        Engine::Get()->SafeReleaseHandle<Shader>(std::move(m_shader));
    }

    m_shader = std::move(shader);
    
    if (m_shader) {
        if (IsInitCalled()) {
            InitObject(m_shader);
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
            InitObject(m_material);
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

    m_previous_transform_matrix = m_transform.GetMatrix();
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
void Entity::OnAddedToPipeline(RenderGroup *pipeline)
{
    std::lock_guard guard(m_render_instances_mutex);

    m_render_groups.Insert(pipeline);
}

void Entity::OnRemovedFromPipeline(RenderGroup *pipeline)
{
    std::lock_guard guard(m_render_instances_mutex);

    if (pipeline == m_primary_renderer_instance.renderer_instance) {
        m_primary_renderer_instance = {
            .renderer_instance = nullptr,
            .changed  = true
        };
    }

    m_render_groups.Erase(pipeline);
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


void Entity::AddToOctree(Octree &octree)
{
    AssertThrow(m_octree == nullptr);

    if (!octree.Insert(this)) {
        DebugLog(LogType::Warn, "Entity #%lu could not be added to octree\n", m_id.value);
    }
}

void Entity::RemoveFromOctree()
{
    DebugLog(
        LogType::Debug,
        "Remove entity #%u from octree\n",
        GetID().value
    );

    m_octree->OnEntityRemoved(this);
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
    if (!Engine::Get()->GetGPUDevice()->GetFeatures().IsRaytracingEnabled()) {
        return false;
    }
#else
    return false;
#endif

    if (!IsRenderable()) {
        // needs mesh, material, etc.
        return false;
    }

    m_blas = CreateObject<BLAS>(
        m_id,
        Handle<Mesh>(m_mesh),
        Handle<Material>(m_material),
        m_transform
    );

    if (IsInitCalled()) {
        if (InitObject(m_blas)) {
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
        SetFlags(InitInfo::ENTITY_FLAGS_HAS_BLAS);
        
        return true;
    }
}

} // namespace hyperion::v2
