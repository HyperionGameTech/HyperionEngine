#include "AabbDebugController.hpp"
#include <Engine.hpp>
#include <util/MeshBuilder.hpp>

#include <util/UTF8.hpp>

namespace hyperion::v2 {

AABBDebugController::AABBDebugController()
    : Controller(false)
{
}

void AABBDebugController::OnAttachedToScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        scene->AddEntity(m_aabb_entity);
    }
}

void AABBDebugController::OnDetachedFromScene(ID<Scene> id)
{
    if (!m_aabb_entity) {
        return;
    }

    if (auto scene = Handle<Scene>(id)) {
        scene->RemoveEntity(m_aabb_entity);
    }
}

void AABBDebugController::OnAdded()
{
    m_aabb = GetOwner()->GetWorldAABB();

    Handle<Mesh> mesh = MeshBuilder::Cube();
    VertexAttributeSet vertex_attributes = mesh->GetVertexAttributes();

    const ShaderProperties shader_properties(vertex_attributes);

    Material::ParameterTable material_parameters = Material::DefaultParameters();
    material_parameters.Set(Material::MATERIAL_KEY_ALBEDO, Material::Parameter(Color(1.0f, 0.0f, 0.0f, 1.0f)));

    Handle<Material> material = Engine::Get()->GetMaterialCache().GetOrCreate(
        MaterialAttributes {
            .bucket = Bucket::BUCKET_TRANSLUCENT,
            .fill_mode = FillMode::LINE,
            .cull_faces = FaceCullMode::NONE,
            .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST
        },
        material_parameters
    );

    Handle<Shader> shader = Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(Forward), shader_properties);

    m_aabb_entity = CreateObject<Entity>(
        std::move(mesh),
        std::move(shader),
        std::move(material)
    );

    m_aabb_entity->SetFlags(Entity::InitInfo::ENTITY_FLAGS_RAY_TESTS_ENABLED, false);
    m_aabb_entity->SetFlags(Entity::InitInfo::ENTITY_FLAGS_INCLUDE_IN_INDIRECT_LIGHTING, false);
    m_aabb_entity->SetFlags(Entity::InitInfo::ENTITY_FLAGS_HAS_BLAS, false);

    InitObject(m_aabb_entity);
}

void AABBDebugController::OnRemoved()
{
    if (m_aabb_entity) {
        const auto scenes = m_aabb_entity->GetScenes();

        for (const auto &id : scenes) {
            m_aabb_entity->SetIsInScene(id, false);
        }

        m_aabb_entity.Reset();
    }
}

void AABBDebugController::OnTransformUpdate(const Transform &transform)
{
    m_aabb = GetOwner()->GetWorldAABB();

    if (m_aabb_entity == nullptr) {
        DebugLog(
            LogType::Warn,
            "No AABB entity set!\n"
        );

        return;
    }

    m_aabb_entity->SetTransform(Transform(
        m_aabb.GetCenter(),
        m_aabb.GetExtent() * 0.5f,
        Quaternion::Identity()
    ));
}

} // namespace hyperion::v2