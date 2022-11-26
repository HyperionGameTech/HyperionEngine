#include "AabbDebugController.hpp"
#include <Engine.hpp>
#include <util/MeshBuilder.hpp>

#include <util/UTF8.hpp>

namespace hyperion::v2 {

AABBDebugController::AABBDebugController()
    : Controller("AABBDebugController", false)
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
            GetOwner()->GetID().value
        );

        return;
    }

    auto mesh = MeshBuilder::Cube();
    auto vertex_attributes = mesh->GetVertexAttributes();

    auto material = Engine::Get()->CreateObject<Material>("aabb_material");

    auto shader = Engine::Get()->shader_manager.GetShader(ShaderManager::Key::DEBUG_AABB);

    m_aabb_entity = Engine::Get()->CreateObject<Entity>(
        std::move(mesh),
        std::move(shader),
        std::move(material),
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = vertex_attributes
            },
            MaterialAttributes {
                .bucket = Bucket::BUCKET_TRANSLUCENT,
                .fill_mode = FillMode::LINE,
                .cull_faces = FaceCullMode::NONE,
                .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_ALPHA_BLENDING
            }
        ),
        Entity::InitInfo {
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