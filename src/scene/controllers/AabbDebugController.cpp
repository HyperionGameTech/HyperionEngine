#include "AABBDebugController.hpp"
#include <Engine.hpp>
#include <builders/MeshBuilder.hpp>

#include <util/UTF8.hpp>

namespace hyperion::v2 {

AABBDebugController::AABBDebugController(Engine *engine)
    : Controller("AABBDebugController", false),
      m_engine(engine)
{
}

void AABBDebugController::OnAdded()
{
    auto *scene = GetOwner()->GetScene();
    m_aabb = GetOwner()->GetWorldAABB();

    if (scene == nullptr) {
        DebugLog(
            LogType::Error,
            "Added aabb debug controller but Entity #%u was not in scene\n",
            GetOwner()->GetId().value
        );

        return;
    }

    AssertThrow(m_engine != nullptr);

    auto mesh = MeshBuilder::Cube();
    auto vertex_attributes = mesh->GetVertexAttributes();
    auto material = GetEngine()->CreateHandle<Material>("aabb_material");

    auto shader = m_engine->shader_manager.GetShader(ShaderManager::Key::DEBUG_AABB);
    const auto shader_id = shader != nullptr ? shader->GetId() : Shader::empty_id;

    m_aabb_entity = GetEngine()->CreateHandle<Entity>(
        GetEngine()->CreateHandle<Mesh>(mesh.release()),
        std::move(shader),
        std::move(material),
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = vertex_attributes,
                .fill_mode = FillMode::LINE,
                .cull_faces = FaceCullMode::NONE
            },
            MaterialAttributes {
                .bucket = Bucket::BUCKET_TRANSLUCENT,
                .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_ALPHA_BLENDING
            },
            shader_id
        ),
        Entity::ComponentInitInfo {
            .flags = 0x0 // no flags
        }
    );

    scene->AddEntity(Handle<Entity>(m_aabb_entity));
}

void AABBDebugController::OnRemoved()
{
    if (m_aabb_entity != nullptr) {
        if (auto *scene = m_aabb_entity->GetScene()) {
            scene->RemoveEntity(m_aabb_entity);
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