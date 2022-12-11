#include <scene/skydome/controllers/SkydomeController.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

SkydomeController::SkydomeController()
    : Controller("SkydomeController")
{
}

void SkydomeController::OnAdded()
{
    auto dome_node = Engine::Get()->GetAssetManager().Load<Node>("models/dome.obj");
    m_dome = dome_node[0].GetEntity();

    if (m_dome) {
        m_dome->SetScale(50.0f);
        
        Handle<Material> material = CreateObject<Material>();
        material->SetBucket(Bucket::BUCKET_SKYBOX);
        // material->SetBlendMode(BlendMode::NORMAL);
        material->SetFaceCullMode(FaceCullMode::NONE);
        material->SetIsDepthTestEnabled(false);
        material->SetIsDepthWriteEnabled(false);

        m_dome->SetMaterial(std::move(material));
        m_dome->SetShader(CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("Skydome")));
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

} // namespace hyperion::v2
