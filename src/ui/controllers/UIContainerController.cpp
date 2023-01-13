#include <ui/controllers/UIContainerController.hpp>
#include <ui/UIText.hpp>
#include <Engine.hpp>
#include <core/Containers.hpp>
#include <math/Transform.hpp>
#include <util/MeshBuilder.hpp>

namespace hyperion::v2 {

UIContainerController::UIContainerController()
    : UIGridController(true)
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
    GetOwner()->SetShader(Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(UIObject)));

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

inline bool UIContainerController::IsMouseWithinHandle(const Vector2 &mouse, const Vector4 &handle)
{
    return ((mouse.x >= handle.x && mouse.x <= handle.z) && (mouse.y >= handle.y && mouse.y <= handle.w));
}

bool UIContainerController::TransformHandle(const Vector4 &bounds, Extent2D direction)
{
    const Vector2 pixel_size = m_attached_camera->GetPixelSize();

    Vector2 click = m_mouse_click_position;
    // Check if point is in bounds with handle
    if ((!m_drag_direction.x && !m_drag_direction.y) && IsMouseWithinHandle(click, bounds)) {
        m_drag_direction = direction;
    }

    const Extent2D direction_diff = (m_drag_direction - direction);

    if (direction_diff.x == 0 && direction_diff.y == 0) {
        return false;
    }

    Transform transform = GetOwner()->GetTransform();
    Vector3 translation = transform.GetTranslation();
    Vector3 scale = transform.GetScale();

    float divisor = (m_drag_direction.x && m_drag_direction.y) ? 4.0f : 2.0f;

    if (m_drag_direction.x) {
        const float horizontal_difference = (m_mouse_last_click.x - m_mouse_click_position.x) / divisor;
        translation.x -= horizontal_difference;
        scale.x -= horizontal_difference;
    }
    if (m_drag_direction.y) {
        const float vertical_difference = (m_mouse_last_click.y - m_mouse_click_position.y) / divisor;
        translation.y -= vertical_difference;
        scale.y -= vertical_difference;
    }

    transform.SetScale(scale);
    transform.SetTranslation(translation);
    GetOwner()->SetTransform(transform);

    return true;
}

Vector4 UIContainerController::GetHandleRect(const UIContainerHandleType handle_type)
{
    const Vector3 bounding_max = GetOwner()->GetWorldAABB().GetMax();
    const Vector3 bounding_min = GetOwner()->GetWorldAABB().GetMin();
    const Vector3 bounding_extent = GetOwner()->GetWorldAABB().GetExtent();

    Float handle_width = GetHandleThickness();

    switch (handle_type) {
        case UI_HANDLE_CORNER:
            return Vector4 {
                Vector2(bounding_max.x - handle_width,  bounding_max.y - handle_width),
                Vector2(bounding_max.x, bounding_max.y)
            };
        case UI_HANDLE_BOTTOM:
            return Vector4 {
                Vector2(bounding_min.x,  bounding_max.y - handle_width),
                Vector2(bounding_max.x - handle_width, bounding_max.y)
            };
        case UI_HANDLE_RIGHT:
            return Vector4 {
                Vector2(bounding_max.x - handle_width, bounding_min.y),
                Vector2(bounding_max.x, bounding_max.y - handle_width)
            };
        default:
            break;
    }
    return Vector4::zero;
}

void UIContainerController::CheckResizeHovering(const UIEvent &event)
{
    const Vector2 &mouse = event.GetMousePosition();
    const ApplicationWindow *window = event.GetWindow();
    if (IsMouseWithinHandle(mouse, GetHandleRect(UI_HANDLE_BOTTOM))) {
        window->SetCursor(SystemCursorType::SYSTEM_CURSOR_SIZE_VERTICAL);
    } else if (IsMouseWithinHandle(mouse, GetHandleRect(UI_HANDLE_RIGHT))) {
        window->SetCursor(SystemCursorType::SYSTEM_CURSOR_SIZE_HORIZONTAL);
    } else if (IsMouseWithinHandle(mouse, GetHandleRect(UI_HANDLE_CORNER))) {
        window->SetCursor(SystemCursorType::SYSTEM_CURSOR_SIZE_NWSE);
    } else {
        window->SetCursor(SystemCursorType::SYSTEM_CURSOR_DEFAULT);
    }
}

void UIContainerController::CheckContainerResize()
{
    bool bottom = TransformHandle(GetHandleRect(UI_HANDLE_BOTTOM), { 0, 1 });
    bool right = TransformHandle(GetHandleRect(UI_HANDLE_RIGHT), { 1, 0 });

    if (bottom && right) {
        TransformHandle(GetHandleRect(UI_HANDLE_CORNER), { 1, 1 });
    }
}

void UIContainerController::OnEvent(const UIEvent &event)
{
    m_mouse_click_position = event.GetMousePosition();

    if (event.type == UIEvent::Type::MOUSE_DOWN) {
        m_mouse_last_click = m_mouse_click_position;
    } else if (event.type == UIEvent::Type::MOUSE_UP) {
        m_drag_direction = { 0, 0 };
    } else if (event.type == UIEvent::Type::MOUSE_DRAG) {
        CheckContainerResize();
        m_mouse_last_click = m_mouse_click_position;
    } else if (event.type == UIEvent::Type::MOUSE_HOVER) {
        CheckResizeHovering(event);
    } else if (event.type == UIEvent::Type::MOUSE_HOVER_LOST) {
        const ApplicationWindow *window = event.GetWindow();
        window->SetCursor(SystemCursorType::SYSTEM_CURSOR_DEFAULT);
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