#include <ui/controllers/UIButtonController.hpp>
#include <ui/UIText.hpp>
#include <Engine.hpp>
#include <core/Containers.hpp>
#include <math/Transform.hpp>
#include <util/MeshBuilder.hpp>

namespace hyperion::v2 {

UIButtonController::UIButtonController()
    : UIController(false)
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
    auto font_texture = g_asset_manager->Load<Texture>("textures/fontmap.png");
            
    FontMap font_map(
        font_texture,
        Extent2D { 32, 32 }
    );

    GetOwner()->SetMesh(UIText::BuildTextMesh(font_map, "HyperionEngine v0.2"));

    auto mat = g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition = ShaderDefinition {
                HYP_NAME(UIObject),
                ShaderProperties(GetOwner()->GetMesh()->GetVertexAttributes())
            },
            .bucket = Bucket::BUCKET_UI,
            .blend_mode = BlendMode::NORMAL,
            .cull_faces = FaceCullMode::NONE,
        },
        { },
        {
            { Material::MATERIAL_TEXTURE_ALBEDO_MAP, font_texture }
        }
    );

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
    } else if (event.type == UIEvent::Type::MOUSE_DRAG) {
        std::cout << "Mouse drag on" << GetOwner()->GetID().value << "\n";
    } else if (event.type == UIEvent::Type::MOUSE_HOVER) {
        std::cout << "Mouse hover on" << GetOwner()->GetID().value << "\n";
    }

    if (HasScript() && IsScriptValid()) {
        m_script->CallFunction(
            m_script_methods[SCRIPT_METHOD_0],
            m_self_object,
            Int(event.type),
            m_script->CreateInternedObject<Vector2>(event.GetMousePosition())
        );
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