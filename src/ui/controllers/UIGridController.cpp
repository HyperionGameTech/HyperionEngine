//
// Created by emd22 on 13/12/22.
//

#include <ui/controllers/UIGridController.hpp>
#include <Engine.hpp>
#include <core/Containers.hpp>
#include <math/Transform.hpp>
#include <util/MeshBuilder.hpp>

namespace hyperion::v2 {

UIGridController::UIGridController(const String &name, bool recieves_update)
    : UIController(name, recieves_update),
      m_grid_divisions(10, 10),
      m_cell_size(0, 0)
{
}

bool UIGridController::CreateScriptedMethods()
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

void UIGridController::OnAdded()
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

void UIGridController::OnRemoved()
{
    Controller::OnRemoved();
}

void UIGridController::OnUpdate(GameCounter::TickUnit delta)
{
    Controller::OnUpdate(delta);
}

void UIGridController::OnTransformUpdate(const Transform &transform)
{
    const auto world_transform = GetOwner()->GetWorldAABB();
    m_cell_size = Vector2(world_transform.GetExtent()) / Vector2((Float)m_grid_divisions.x, (Float)m_grid_divisions.y);

    const auto &children = GetOwner()->GetParent()->GetChildren();

    for (auto &child : children) {
        const ControllerSet &controllers = child.GetEntity()->GetControllers();

        for (const auto &it : controllers) {
            if (auto *controller = dynamic_cast<UIController *>(it.second.Get())) {
                const Extent2D &grid = controller->GetGridOffset();
                Vector3 new_offset = world_transform.GetMin() + (Vector3((Float)grid.x, (Float)grid.y, 0.0f) * Vector3(m_cell_size, 0.0f));
                std::cout << "Offset: " << new_offset << "\n";
                child.GetEntity()->SetTranslation(new_offset);
            }
        }
    }
    std::cout << "update grid" << m_cell_size << "\n";
}

} // namespace hyperion::v2
