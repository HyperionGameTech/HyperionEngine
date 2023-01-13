#include <ui/controllers/UIContainerController.hpp>
#include <ui/UIText.hpp>
#include <Engine.hpp>
#include <core/Containers.hpp>
#include <math/Transform.hpp>
#include <util/MeshBuilder.hpp>

namespace hyperion::v2 {

UIContainerController::UIContainerController()
    : UIGridController("UIContainerController", true)
{
}

bool UIContainerController::CreateScriptedMethods()
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

void UIContainerController::OnAdded()
{
    GetOwner()->SetMesh(MeshBuilder::Quad());
    GetOwner()->SetShader(Handle<Shader>(Engine::Get()->shader_manager.GetShader(ShaderManager::Key::BASIC_UI)));

    auto mat = CreateObject<Material>();
    mat->SetBucket(Bucket::BUCKET_UI);
    mat->SetFaceCullMode(FaceCullMode::NONE);
    mat->SetBlendMode(BlendMode::NORMAL);
    GetOwner()->SetMaterial(std::move(mat));

    Controller::OnAdded();
}

void UIContainerController::OnRemoved()
{
    Controller::OnRemoved();
}


void UIContainerController::OnAttachedToScene(Scene *scene)
{
}

void UIContainerController::OnDetachedFromScene(Scene *scene)
{
}

void UIContainerController::OnEvent(const UIEvent &event)
{
    if (event.type == UIEvent::Type::MOUSE_DOWN) {
    } else if (event.type == UIEvent::Type::MOUSE_UP) {
    } else if (event.type == UIEvent::Type::MOUSE_DRAG) {
    } else if (event.type == UIEvent::Type::MOUSE_HOVER) {
    } else if (event.type == UIEvent::Type::MOUSE_HOVER_LOST) {
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

void UIContainerController::OnUpdate(GameCounter::TickUnit delta)
{
    Controller::OnUpdate(delta);
}

void UIContainerController::OnTransformUpdate(const Transform &transform)
{
    UIGridController::OnTransformUpdate(transform);
}

} // namespace hyperion::v2