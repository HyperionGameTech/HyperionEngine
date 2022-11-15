#include <ui/controllers/UIButtonController.hpp>
#include <ui/UIText.hpp>
#include <Engine.hpp>
#include <core/Containers.hpp>
#include <math/Transform.hpp>
#include <util/MeshBuilder.hpp>

namespace hyperion::v2 {

UIButtonController::UIButtonController()
    : UIController("UIButtonController", true)
{
}

bool UIButtonController::CreateScriptedMethods()
{
    if (!Controller::CreateScriptedMethods()) {
        return false;
    }

    if (!m_script->GetMember(m_self_object, "OnEvent", m_script_methods[SCRIPT_METHOD_0])) {
        DebugLog(
            LogType::Error,
            "Failed to get `OnEvent` method\n"
        );

        return false;
    }

    return true;
}

void UIButtonController::OnAdded()
{
    auto font_texture = GetEngine()->GetAssetManager().Load<Texture>("textures/fontmap.png");
            
    FontMap font_map(
        font_texture,
        Extent2D { 32, 32 }
    );

    GetOwner()->SetMesh(GetEngine()->CreateHandle<Mesh>(UIText::BuildTextMesh(font_map, "hello world")));
    GetOwner()->SetShader(Handle<Shader>(GetEngine()->shader_manager.GetShader(ShaderManager::Key::BASIC_UI)));

    auto mat = GetEngine()->CreateHandle<Material>();
    mat->SetBucket(Bucket::BUCKET_UI);
    mat->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle(font_map.GetTexture()));
    mat->SetFaceCullMode(renderer::FaceCullMode::NONE);
    mat->SetIsAlphaBlended(true);
    GetOwner()->SetMaterial(std::move(mat));

    Controller::OnAdded();
}

void UIButtonController::OnRemoved()
{
    Controller::OnRemoved();
}

void UIButtonController::OnEvent(const UIEvent &event)
{
    if (event.type == UIEvent::Type::MOUSE_DOWN) {
        std::cout << "Mouse down on" << GetOwner()->GetID().value << "\n";
    } else if (event.type == UIEvent::Type::MOUSE_UP) {
        std::cout << "Mouse up on" << GetOwner()->GetID().value << "\n";
    }

    if (HasScript() && IsScriptValid()) {
        m_script->CallFunction(m_script_methods[SCRIPT_METHOD_0], m_self_object, Int(event.type));
    }
}

void UIButtonController::OnUpdate(GameCounter::TickUnit delta)
{
    Controller::OnUpdate(delta);
}

void UIButtonController::OnTransformUpdate(const Transform &transform)
{
}

} // namespace hyperion::v2