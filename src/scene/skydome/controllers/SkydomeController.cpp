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
    m_dome = dome_node[0];

    if (m_dome) {
        m_dome.Scale(50.0f);
        
        if (Handle<Entity> &entity = m_dome.Get()->GetEntity()) {
            Handle<Material> material = CreateObject<Material>();
            material->SetBucket(Bucket::BUCKET_SKYBOX);
            // material->SetBlendMode(BlendMode::NORMAL);
            material->SetFaceCullMode(FaceCullMode::NONE);
            material->SetIsDepthTestEnabled(false);
            material->SetIsDepthWriteEnabled(false);

            entity->SetMaterial(std::move(material));
            entity->SetShader(CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("Skydome")));
        }
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
    m_dome.Remove();
}

void SkydomeController::OnAttachedToScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        scene->GetRoot().AddChild(m_dome);
    }
}

} // namespace hyperion::v2
