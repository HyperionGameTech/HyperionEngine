#include <ui/controllers/UIContainerController.hpp>
#include <ui/UIText.hpp>
#include <Engine.hpp>
#include <core/Containers.hpp>
#include <math/Transform.hpp>
#include <util/MeshBuilder.hpp>

namespace hyperion::v2 {

UIContainerController::UIContainerController()
    : UIController("UIContainerController", true)
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


void UIContainerController::OnAttachedToScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        if (scene->IsWorldScene()) {
            m_attached_camera = scene->GetCamera();
        }
    }
}

void UIContainerController::OnDetachedFromScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        if (scene->GetCamera() == m_attached_camera) {
            m_attached_camera.Reset();
        }
    }
}

void UIContainerController::TransformHandle(const Vector4 bounds, bool check_horizontal, bool check_vertical)
{
    const Vector2 pixel_size = m_attached_camera->GetPixelSize();

    Vector2 click = m_mouse_click_position;
    std::cout << "POSITIONS: " << click.x << "," << click.y << " || " << bounds << "\n";
    // Check if point is in bounds with handle
    if ((click.x >= bounds.x && click.x <= bounds.z) &&
        (click.y >= bounds.y && click.y <= bounds.w))
    {
        Transform transform = GetOwner()->GetTransform();
        Vector3 scale = transform.GetScale();
        Vector3 translation = transform.GetTranslation();

        std::cout << "POOPIES CHECK POOPIES CHECK\n\n";

        if (check_horizontal) {
            float diffx = (m_mouse_last_click.x - m_mouse_click_position.x)*0.5;
            translation.x -= diffx;
            scale.x -= diffx;
        }
        if (check_vertical) {
            float diffy = (m_mouse_last_click.y - m_mouse_click_position.y)*0.5;
            translation.y -= diffy;
            scale.y -= diffy;
        }
        transform.SetScale(scale);
        transform.SetTranslation(translation);
        GetOwner()->SetTransform(transform);
    }
}

void UIContainerController::CheckTransformHandles()
{
    if (!m_attached_camera.IsValid()) {
        return;
    }

    constexpr Float handle_thickness = 0.1f;

    const Vector2 pixel_size = m_attached_camera->GetPixelSize();

    // top right
    const Vector3 bounding_max = GetOwner()->GetWorldAABB().GetMax();
    // bottom left
    const Vector3 bounding_min = GetOwner()->GetWorldAABB().GetMin();

    /*Vector4 horizontal_bounds {
        Vector2(bounding_min.x, bounding_min.y + handle_thickness),
        Vector2(GetOwner()->GetWorldAABB().GetExtent().x, handle_thickness)
    };

    Vector4 vertical_bounds {
        Vector2(bounding_max.x - handle_thickness, bounding_min.y),
        Vector2(handle_thickness, GetOwner()->GetWorldAABB().GetExtent().y)
    };*/
    Vector4 bounds {
        Vector2(bounding_min.x, bounding_min.y),
        Vector2(bounding_min.x + GetOwner()->GetWorldAABB().GetExtent().x, bounding_min.y + GetOwner()->GetWorldAABB().GetExtent().y),
    };

    TransformHandle(bounds, true, true);
    /*TransformHandle(horizontal_bounds, true, false);
    TransformHandle(vertical_bounds, false, true);*/
}

void UIContainerController::OnEvent(const UIEvent &event)
{
    m_mouse_click_position = event.GetMousePosition();

    if (event.type == UIEvent::Type::MOUSE_DOWN) {
        m_mouse_last_click = m_mouse_click_position;
        //std::cout << "Mouse down on " << GetOwner()->GetID().value << "\n";
    } else if (event.type == UIEvent::Type::MOUSE_UP) {
        //std::cout << "Mouse up on " << GetOwner()->GetID().value << "\n";
    } else if (event.type == UIEvent::Type::MOUSE_DRAG) {
        //std::cout << "Mouse drag on " << GetOwner()->GetID().value << "\n";
        CheckTransformHandles();
        m_mouse_last_click = m_mouse_click_position;
    } else if (event.type == UIEvent::Type::MOUSE_HOVER) {
        //std::cout << "Mouse hover on " << GetOwner()->GetID().value << "\n";
    }

    if (HasScript() && IsScriptValid()) {
        m_script->CallFunction(
            m_script_methods[SCRIPT_METHOD_0],
            m_self_object,
            Int(event.type),
            m_script->CreateInternedObject<Vector2>(m_mouse_click_position)
        );
    }
}

void UIContainerController::OnUpdate(GameCounter::TickUnit delta)
{
    Controller::OnUpdate(delta);
}

void UIContainerController::OnTransformUpdate(const Transform &transform)
{
}

} // namespace hyperion::v2