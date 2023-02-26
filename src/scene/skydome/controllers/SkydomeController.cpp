#include <scene/skydome/controllers/SkydomeController.hpp>
#include <asset/serialization/fbom/FBOMObject.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

SkydomeController::SkydomeController()
    : Controller()
{
}

void SkydomeController::OnAdded()
{
    auto dome_node = g_asset_manager->Load<Node>("models/dome.obj");
    m_dome = dome_node[0].GetEntity();

    if (m_dome) {
        m_dome->SetFlags(Entity::InitInfo::ENTITY_FLAGS_HAS_BLAS, false);
        m_dome->SetScale(150.0f);
        
        Handle<Material> material = CreateObject<Material>();
        material->SetBucket(Bucket::BUCKET_SKYBOX);
        // material->SetBlendMode(BlendMode::NORMAL);
        material->SetFaceCullMode(FaceCullMode::NONE);
        material->SetIsDepthTestEnabled(false);
        material->SetIsDepthWriteEnabled(false);

        m_dome->SetMaterial(std::move(material));
        m_dome->SetShader(g_shader_manager->GetOrCreate(
            HYP_NAME(Skydome),
            ShaderProperties(renderer::static_mesh_vertex_attributes)
        ));
    }
}

void SkydomeController::OnRemoved()
{
}

void SkydomeController::OnUpdate(GameCounter::TickUnit delta)
{
}

void SkydomeController::OnDetachedFromScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        scene->RemoveEntity(m_dome);
    }
}

void SkydomeController::OnAttachedToScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        scene->AddEntity(m_dome);
    }
}

void SkydomeController::Serialize(fbom::FBOMObject &out) const
{
    out.SetProperty("controller_name", fbom::FBOMString(), Memory::StrLen(controller_name), controller_name);
}

fbom::FBOMResult SkydomeController::Deserialize(const fbom::FBOMObject &in)
{
    return fbom::FBOMResult::FBOM_OK;
}

} // namespace hyperion::v2
