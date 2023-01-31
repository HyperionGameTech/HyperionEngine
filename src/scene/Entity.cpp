#include "Entity.hpp"
#include <math/BoundingSphere.hpp>
#include <rendering/Renderer.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/RenderCommands.hpp>

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>

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
    m_renderable_attributes(renderable_attributes),
    m_octree(nullptr),
    m_needs_octree_update(false),
    m_shader_data_state(ShaderDataState::DIRTY)
{
    if (m_mesh) {
        m_local_aabb = m_mesh->CalculateAABB();
        
        if (!m_local_aabb.Empty()) {
            m_world_aabb = BoundingBox(m_transform.GetMatrix() * m_local_aabb.min, m_transform.GetMatrix() * m_local_aabb.max);
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

    // if (!m_shader) {
    //     SetShader(Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(Forward)));
    // }

    m_draw_proxy.entity_id = m_id;
    m_previous_transform_matrix = m_transform.GetMatrix();

    InitObject(m_shader);
    InitObject(m_material);
    InitObject(m_skeleton);

    if (Engine::Get()->GetConfig().Get(CONFIG_RT_ENABLED)) {
        if (InitObject(m_blas)) {
            SetFlags(InitInfo::ENTITY_FLAGS_HAS_BLAS);
        } else if (HasFlags(InitInfo::ENTITY_FLAGS_HAS_BLAS)) {
            CreateBLAS();
        }
    }

    // set our aabb to be the mesh aabb now that it is init
    if (InitObject(m_mesh)) {
        m_local_aabb = m_mesh->CalculateAABB();
        m_world_aabb = BoundingBox::empty;
        
        if (!m_local_aabb.Empty()) {
            for (const auto &corner : m_local_aabb.GetCorners()) {
                m_world_aabb.Extend(m_transform.GetMatrix() * corner);
            }
        }

        m_needs_octree_update = true;
    }

    RebuildRenderableAttributes();

    SetReady(true);
    
    OnTeardown([this]() {
        DebugLog(
            LogType::Debug,
            "Destroy entity with id %u, with name hash %llu\n",
            GetID().value,
            GetName().GetHashCode().Value()
        );

        SetReady(false);

        // remove all controllers
        for (auto &it : m_controllers) {
            auto &controller = it.second;

            if (!controller) {
                continue;
            }

            for (Node *node : m_nodes) {
                controller->OnDetachedFromNode(node);
            }

            for (const ID<Scene> &id : m_scenes) {
                controller->OnDetachedFromScene(id);
            }

            controller->OnRemoved();
        }

        m_controllers.Clear();

        m_material.Reset();
        m_blas.Reset();

        m_skeleton.Reset();
        m_mesh.Reset();
        m_shader.Reset();

        HYP_SYNC_RENDER();
    });
}

void Entity::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    if (m_skeleton && m_skeleton->IsReady()) {
        m_skeleton->Update(delta);
    }

    if (m_material && m_material->IsReady()) {
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
            // UpdateOctree();
        }

        const VisibilityState &octree_visibility_state = m_octree->GetVisibilityState();
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

    EntityDrawProxy draw_proxy {
        .entity_id = m_id,
        .mesh_id = mesh_id,
        .material_id = material_id,
        .skeleton_id = skeleton_id,
        .bounding_box = m_world_aabb,
        .mesh = m_mesh.Get(),
        .bucket = m_renderable_attributes.material_attributes.bucket
    };

    RenderCommands::Push<RENDER_COMMAND(UpdateEntityRenderData)>(
        this,
        std::move(draw_proxy),
        m_transform.GetMatrix(),
        m_previous_transform_matrix
    );

    if (Memory::Compare(&m_previous_transform_matrix, &m_transform.GetMatrix(), sizeof(Matrix4)) == 0) {
        m_shader_data_state = ShaderDataState::CLEAN;
    }

    Memory::Copy(&m_previous_transform_matrix, &m_transform.GetMatrix(), sizeof(Matrix4));
}

void Entity::UpdateOctree()
{
    // AssertReady();

    if (Octree *octree = m_octree) {
        const Octree::Result update_result = octree->Update(this);

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

    m_mesh = std::move(mesh);

    if (m_mesh) {
        m_local_aabb = m_mesh->CalculateAABB();
        m_world_aabb = BoundingBox::empty;

        //if (m_nodes.Any()) {
        //    for (Node *node : m_nodes) {
        //        node->UpdateWorldTransform();
        //    }
        //}

        if (!m_local_aabb.Empty()) {
            for (const auto &corner : m_local_aabb.GetCorners()) {
                m_world_aabb.Extend(m_transform.GetMatrix() * corner);
            }
        }
    }

    if (IsInitCalled()) {
        InitObject(m_mesh);

        UpdateOctree();
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
        new_renderable_attributes.shader_def = m_shader->GetCompiledShader().GetDefinition();
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

void Entity::SetIsAttachedToNode(Node *node, bool is_attached_to_node)
{
    const bool is_currently_attached_to_node = m_nodes.Contains(node);

    if (is_attached_to_node == is_currently_attached_to_node) {
        return;
    }

    if (is_attached_to_node) {
        for (auto &controller : m_controllers) {
            controller.second->OnAttachedToNode(node);
        }

        m_nodes.PushBack(node);
    } else {
        for (auto it = m_nodes.Begin(); it != m_nodes.End();) {
            if (*it == node) {
                for (auto &controller : m_controllers) {
                    AssertThrow(controller.second != nullptr);

                    controller.second->OnDetachedFromNode(node);
                }

                m_nodes.Erase(it);

                break;
            } else {
                ++it;
            }
        }
    }
}

void Entity::SetIsInScene(ID<Scene> id, bool is_in_scene)
{
    if (!id) {
        return;
    }

    const bool has_scene = IsInScene(id);

    if (has_scene == is_in_scene) {
        return;
    }

    Handle<Scene> scene(id);

    if (!scene) {
        return;
    }
    
    if (has_scene) { // removing
        if (auto &scene_tlas = scene->GetTLAS()) {
            if (m_blas) {
                //scene_tlas->RemoveBLAS(m_blas);
            }
        }

        for (auto &controller : m_controllers) {
            AssertThrow(controller.second != nullptr);

            controller.second->OnDetachedFromScene(id);
        }

        m_scenes.Erase(id);
    } else { // adding
        m_scenes.Insert(id);
        
        if (auto &scene_tlas = scene->GetTLAS()) {
            if (m_blas) {
                scene_tlas->AddBLAS(Handle<BLAS>(m_blas));
            }
        }

        for (auto &controller : m_controllers) {
            AssertThrow(controller.second != nullptr);

            controller.second->OnAttachedToScene(id);
        }
    }

    m_shader_data_state |= ShaderDataState::DIRTY;
}

bool Entity::IsVisibleToCamera(ID<Camera> camera_id) const
{
    const VisibilityState &parent_visibility_state = Engine::Get()->GetWorld()->GetOctree().GetVisibilityState();
    const UInt8 visibility_cursor = Engine::Get()->GetWorld()->GetOctree().LoadVisibilityCursor();

    return m_visibility_state.ValidToParent(parent_visibility_state, visibility_cursor)
        && m_visibility_state.Get(camera_id, visibility_cursor);
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
        new_renderable_attributes.mesh_attributes.vertex_attributes |= renderer::skeleton_vertex_attributes;
    }

    if (m_shader && m_mesh) {
        const VertexAttributeSet shader_vertex_attributes = m_shader->GetCompiledShader().GetProperties().GetRequiredVertexAttributes();

        if (shader_vertex_attributes != new_renderable_attributes.mesh_attributes.vertex_attributes) {
            ShaderProperties modified_shader_properties(m_shader->GetCompiledShader().GetProperties());
            modified_shader_properties.SetRequiredVertexAttributes(new_renderable_attributes.mesh_attributes.vertex_attributes);

            DebugLog(
                LogType::Debug,
                "Entity #%u vertex attributes do not match shader #%u vertex attributes, grabbing new shader\n",
                GetID().Value(),
                m_shader->GetID().Value()
            );

            m_shader = Engine::Get()->GetShaderManager().GetOrCreate(
                m_shader->GetName(),
                modified_shader_properties
            );

            InitObject(m_shader);

            AssertThrowMsg(
                m_shader.IsValid(),
                "Invalid shader after grabbing new one because vertex attributes did not match requirements!"
            );
        }

        new_renderable_attributes.shader_def = m_shader->GetCompiledShader().GetDefinition();
    } else {
        new_renderable_attributes.shader_def = { };
    }

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
    if (m_nodes.Any()) {
        for (Node *node : m_nodes) {
            // indirectly calls SetTransform() on this
            node->SetWorldTranslation(translation);
        }
    } else {
        Transform new_transform(m_transform);
        new_transform.SetTranslation(translation);

        SetTransform(new_transform);
    }
}

void Entity::SetScale(const Vector3 &scale)
{
    if (m_nodes.Any()) {
        for (Node *node : m_nodes) {
            // indirectly calls SetTransform() on this
            node->SetWorldScale(scale);
        }
    } else {
        Transform new_transform(m_transform);
        new_transform.SetScale(scale);

        SetTransform(new_transform);
    }
}

void Entity::SetRotation(const Quaternion &rotation)
{
    if (m_nodes.Any()) {
        for (Node *node : m_nodes) {
            // indirectly calls SetTransform() on this
            node->SetWorldRotation(rotation);
        }
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
    
    m_world_aabb = BoundingBox::empty;
    
    if (!m_local_aabb.Empty()) {
        for (const auto &corner : m_local_aabb.GetCorners()) {
            m_world_aabb.Extend(m_transform.GetMatrix() * corner);
        }
    }

    for (auto &it : m_controllers) {
        it.second->OnTransformUpdate(m_transform);
    }

    //for (Node *node : m_nodes) {
    //     node->SetWorldTransform(transform, false)
   // }

    if (IsInitCalled()) {
        UpdateOctree();

        if (m_blas) {
            m_blas->SetTransform(m_transform);
        }
    } else {
        // m_needs_octree_update = true;

        UpdateOctree();
    }
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
        // m_needs_octree_update = true;

        UpdateOctree();
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
        // m_needs_octree_update = true;
        UpdateOctree();
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
    if (m_blas) {
        return true;
    }

#if defined(HYP_FEATURES_ENABLE_RAYTRACING) && HYP_FEATURES_ENABLE_RAYTRACING
    if (!Engine::Get()->GetConfig().Get(CONFIG_RT_ENABLED)) {
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

            for (const auto &scene_id : m_scenes) {
                if (auto scene = Handle<Scene>(scene_id)) {
                    if (auto &tlas = scene->GetTLAS()) {
                        tlas->AddBLAS(Handle<BLAS>(m_blas));
                    }
                }
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
