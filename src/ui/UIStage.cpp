/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <ui/UIStage.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIText.hpp>

#include <util/MeshBuilder.hpp>

#include <math/MathUtil.hpp>

#include <scene/camera/OrthoCamera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <rendering/Texture.hpp>
#include <rendering/font/FontAtlas.hpp>

#include <core/system/AppContext.hpp>
#include <core/system/SystemEvent.hpp>
#include <core/threading/Threads.hpp>
#include <core/logging/Logger.hpp>

#include <input/InputManager.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

UIStage::UIStage(ThreadID owner_thread_id)
    : UIObject(UIObjectType::STAGE),
      m_owner_thread_id(owner_thread_id),
      m_surface_size { 1000, 1000 }
{
    SetName(NAME("Stage"));
    SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
}

UIStage::~UIStage()
{
}

Scene *UIStage::GetScene() const
{
    /*// UIStage parenting - GetScene() returns parent stages' scene
    if (m_stage != nullptr) {
        return m_stage->GetScene();
    }*/

    if (Scene *ui_object_scene = UIObject::GetScene()) {
        return ui_object_scene;
    }

    return m_scene.Get();
}

const RC<FontAtlas> &UIStage::GetDefaultFontAtlas() const
{
    if (m_default_font_atlas != nullptr) {
        return m_default_font_atlas;
    }

    // Parent stage
    if (m_stage != nullptr) {
        return m_stage->GetDefaultFontAtlas();
    }

    return m_default_font_atlas;
}

void UIStage::SetDefaultFontAtlas(RC<FontAtlas> font_atlas)
{
    m_default_font_atlas = std::move(font_atlas);
    
    OnFontAtlasUpdate();
}

void UIStage::Init()
{
    if (const RC<AppContext> &app_context = g_engine->GetAppContext()) {
        const auto UpdateSurfaceSize = [this](ApplicationWindow *window)
        {
            if (window == nullptr) {
                return;
            }

            const Vec2i size = Vec2i(window->GetDimensions());

            m_surface_size = Vec2i(size);

            if (m_scene.IsValid()) {
                m_scene->GetCamera()->SetCameraController(RC<OrthoCameraController>::Construct(
                    0.0f, -float(m_surface_size.x),
                    0.0f, float(m_surface_size.y),
                    float(min_depth), float(max_depth)
                ));
            }
        };

        UpdateSurfaceSize(app_context->GetMainWindow());
        m_on_current_window_changed_handler = app_context->OnCurrentWindowChanged.Bind(UpdateSurfaceSize);
    }

    if (!m_default_font_atlas) {
        auto font_atlas_asset = g_asset_manager->Load<RC<FontAtlas>>("fonts/default.json");

        if (font_atlas_asset.IsOK()) {
            m_default_font_atlas = font_atlas_asset.Result();
        } else {
            HYP_LOG(UI, LogLevel::ERR, "Failed to load default font atlas! Error was: {}", font_atlas_asset.result.message);
        }
    }

    m_scene = CreateObject<Scene>(
        CreateObject<Camera>(),
        m_owner_thread_id,
        Scene::InitInfo {
            Scene::InitInfo::SCENE_FLAGS_NON_WORLD
        }
    );

    m_scene->GetCamera()->SetCameraController(RC<OrthoCameraController>::Construct(
        0.0f, -float(m_surface_size.x),
        0.0f, float(m_surface_size.y),
        float(min_depth), float(max_depth)
    ));

    InitObject(m_scene);

    m_scene->GetRoot()->SetEntity(m_scene->GetEntityManager()->AddEntity());

    m_scene->GetEntityManager()->AddComponent(m_scene->GetRoot()->GetEntity(), UIComponent { RefCountedPtrFromThis() });

    m_scene->GetRoot()->LockTransform();

    SetNodeProxy(m_scene->GetRoot());

    UIObject::Init();
}

void UIStage::AddChildUIObject(UIObject *ui_object)
{
    if (!ui_object) {
        return;
    }

    UIObject::AddChildUIObject(ui_object);

    // Check if no parent stage
    if (m_stage == nullptr) {
        // Set child object stage to this
        ui_object->m_stage = this;
        ui_object->SetAllChildUIObjectsStage(this);
    }
}

void UIStage::Update_Internal(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    UIObject::Update_Internal(delta);

    m_scene->BeginUpdate(delta);
    m_scene->EndUpdate();

    for (auto &it : m_mouse_button_pressed_states) {
        it.second.held_time += delta;
    }
}

void UIStage::OnAttached_Internal(UIObject *parent)
{
    AssertThrow(parent != nullptr);
    AssertThrow(parent->GetNode() != nullptr);

    // UIObject::OnAttached_Internal(parent);

    // Set root to be empty node proxy, now that it is attached to another object.
    m_scene->SetRoot(NodeProxy::empty);
}

void UIStage::OnRemoved_Internal()
{
    // Remove m_stage parent object
    // UIObject::OnRemoved_Internal();

    // Set all sub objects to have a m_stage of this
    // UIObject::SetAllChildUIObjectsStage(this);

    // Re-set scene root to be our node proxy
    m_scene->SetRoot(m_node_proxy);
}

void UIStage::SetStage_Internal(UIStage *stage)
{
    m_stage = stage;
    
    SetNeedsRepaintFlag();
    
    // Do not update children
}

void UIStage::SetOwnerThreadID(ThreadID thread_id)
{
    AssertThrowMsg(thread_id.IsValid(), "Invalid thread ID");

    m_owner_thread_id = thread_id;

    if (m_scene.IsValid()) {
        m_scene->SetOwnerThreadID(thread_id);
    }
}

bool UIStage::TestRay(const Vec2f &position, Array<RC<UIObject>> &out_objects, EnumFlags<UIRayTestFlags> flags)
{
    Threads::AssertOnThread(m_owner_thread_id);

    const Vec4f world_position = m_scene->GetCamera()->TransformScreenToWorld(position);
    const Vec3f direction { world_position.x / world_position.w, world_position.y / world_position.w, 0.0f };

    Ray ray;
    ray.position = world_position.GetXYZ() / world_position.w;
    ray.direction = direction;

    RayTestResults ray_test_results;

    for (auto [entity_id, ui_component, transform_component, bounding_box_component] : m_scene->GetEntityManager()->GetEntitySet<UIComponent, TransformComponent, BoundingBoxComponent>()) {
        if (!ui_component.ui_object) {
            continue;
        }

        if ((flags & UIRayTestFlags::ONLY_VISIBLE) && !ui_component.ui_object->IsVisible()) {
            continue;
        }

        BoundingBox aabb(bounding_box_component.world_aabb);
        aabb.min.z = -1.0f;
        aabb.max.z = 1.0f;
     
        if (aabb.ContainsPoint(direction)) {
            RayHit hit { };
            hit.hitpoint = Vec3f { position.x, position.y, 0.0f };
            hit.distance = -float(ui_component.ui_object->GetComputedDepth());
            hit.id = entity_id.value;

            ray_test_results.AddHit(hit);
        }
    }

    out_objects.Reserve(ray_test_results.Size());

    for (const RayHit &hit : ray_test_results) {
        if (RC<UIObject> ui_object = GetUIObjectForEntity(ID<Entity>(hit.id))) {
            out_objects.PushBack(std::move(ui_object));
        }
    }

    return out_objects.Any();
}

RC<UIObject> UIStage::GetUIObjectForEntity(ID<Entity> entity) const
{
    if (UIComponent *ui_component = m_scene->GetEntityManager()->TryGetComponent<UIComponent>(entity)) {
        return ui_component->ui_object;
    }

    return nullptr;
}

void UIStage::SetFocusedObject(const RC<UIObject> &ui_object)
{
    HYP_LOG(UI, LogLevel::DEBUG, "Set focused UIObject to: {}", ui_object != nullptr ? *ui_object->GetName() : "<none>");

    RC<UIObject> current_focused_ui_object = m_focused_object.Lock();

    // Be sure to set the focused object to nullptr before calling Blur() to prevent infinite recursion
    // due to Blur() calling SetFocusedObject() again.
    m_focused_object.Reset();

    if (current_focused_ui_object != nullptr) {
        if (current_focused_ui_object == ui_object) {
            return;
        }

        // Only blur children if 
        const bool should_blur_children = ui_object == nullptr || !ui_object->IsOrHasParent(current_focused_ui_object);

        current_focused_ui_object->Blur(should_blur_children);
    }

    m_focused_object = ui_object;
}

void UIStage::ComputeActualSize(const UIObjectSize &in_size, Vec2i &out_actual_size, UpdateSizePhase phase, bool is_inner)
{
    // stage with a parent stage: treat self like a normal UIObject
    if (m_stage != nullptr) {
        UIObject::ComputeActualSize(in_size, out_actual_size, phase, is_inner);

        return;
    }

    // inner calculation is the same
    if (is_inner) {
        UIObject::ComputeActualSize(in_size, out_actual_size, phase, is_inner);

        return;
    }

    out_actual_size = m_surface_size;
}

EnumFlags<UIEventHandlerResult> UIStage::OnInputEvent(
    InputManager *input_manager,
    const SystemEvent &event
)
{
    EnumFlags<UIEventHandlerResult> event_handler_result = UIEventHandlerResult::OK;

    RayTestResults ray_test_results;

    const Vec2u window_size = input_manager->GetWindow()->GetDimensions();

    const Vec2i mouse_position = input_manager->GetMousePosition();
    const Vec2i previous_mouse_position = input_manager->GetPreviousMousePosition();
    const Vec2f mouse_screen = Vec2f(mouse_position) / Vec2f(window_size);

    switch (event.GetType()) {
    case SystemEventType::EVENT_MOUSEMOTION:
    {
        // check intersects with objects on mouse movement.
        // for any objects that had mouse held on them,
        // if the mouse is on them, signal mouse movement

        // project a ray into the scene and test if it hits any objects

        const EnumFlags<MouseButtonState> mouse_buttons = input_manager->GetButtonStates();

        if (mouse_buttons != MouseButtonState::NONE) { // mouse drag event
            EnumFlags<UIEventHandlerResult> mouse_drag_event_handler_result = UIEventHandlerResult::OK;

            for (const Pair<ID<Entity>, UIObjectPressedState> &it : m_mouse_button_pressed_states) {
                if (it.second.held_time >= 0.05f) {
                    // signal mouse drag
                    if (RC<UIObject> ui_object = GetUIObjectForEntity(it.first)) {
                        mouse_drag_event_handler_result |= ui_object->OnMouseDrag.Broadcast(MouseEvent {
                            .input_manager      = input_manager,
                            .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                            .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                            .mouse_buttons      = mouse_buttons,
                            .is_down            = true
                        });

                        if (mouse_drag_event_handler_result & UIEventHandlerResult::STOP_BUBBLING) {
                            break;
                        }
                    }
                }
            }

            break;
        }

        Array<RC<UIObject>> ray_test_results;

        if (TestRay(mouse_screen, ray_test_results)) {
            UIObject *first_hit = nullptr;

            EnumFlags<UIEventHandlerResult> mouse_hover_event_handler_result = UIEventHandlerResult::OK;
            EnumFlags<UIEventHandlerResult> mouse_move_event_handler_result = UIEventHandlerResult::OK;

            for (auto it = ray_test_results.Begin(); it != ray_test_results.End(); ++it) {
                if (const RC<UIObject> &ui_object = *it) {
                    if (first_hit) {
                        // We don't want to check the current object if it's not a child of the first hit object,
                        // since it would be behind the first hit object.
                        if (!first_hit->IsOrHasParent(ui_object)) {
                            continue;
                        }
                    } else {
                        first_hit = ui_object;
                    }

                    if (m_hovered_entities.Contains(ui_object->GetEntity())) {
                        // Already hovered, trigger mouse move event instead
                        mouse_move_event_handler_result |= ui_object->OnMouseMove.Broadcast(MouseEvent {
                            .input_manager      = input_manager,
                            .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                            .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                            .mouse_buttons      = mouse_buttons,
                            .is_down            = false
                        });

                        if (mouse_move_event_handler_result & UIEventHandlerResult::STOP_BUBBLING) {
                            break;
                        }
                    }
                }
            }

            first_hit = nullptr;

            for (auto it = ray_test_results.Begin(); it != ray_test_results.End(); ++it) {
                if (const RC<UIObject> &ui_object = *it) {
                    if (first_hit) {
                        // We don't want to check the current object if it's not a child of the first hit object,
                        // since it would be behind the first hit object.
                        if (!first_hit->IsOrHasParent(ui_object)) {
                            continue;
                        }
                    } else {
                        first_hit = ui_object;
                    }

                    if (!m_hovered_entities.Insert(ui_object->GetEntity()).second) {
                        // Already hovered, trigger mouse move event instead
                        event_handler_result |= ui_object->OnMouseMove.Broadcast(MouseEvent {
                            .input_manager      = input_manager,
                            .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                            .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                            .mouse_buttons      = mouse_buttons,
                            .is_down            = false
                        });

                        continue;
                    }

                    ui_object->SetFocusState(ui_object->GetFocusState() | UIObjectFocusState::HOVER);

                    mouse_hover_event_handler_result |= ui_object->OnMouseHover.Broadcast(MouseEvent {
                        .input_manager      = input_manager,
                        .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                        .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                        .mouse_buttons      = mouse_buttons,
                        .is_down            = false
                    });

                    HYP_LOG(UI, LogLevel::DEBUG, "Mouse hover on: {}, Size: {}, Inner size: {}, World AABB: {}, Entity AABB: {}",
                        ui_object->GetName(), ui_object->GetActualSize(), ui_object->GetActualInnerSize(),
                        ui_object->GetWorldAABB(),
                        ui_object->GetNode()->GetEntityAABB());

                    if (mouse_hover_event_handler_result & UIEventHandlerResult::STOP_BUBBLING) {
                        break;
                    }
                }
            }
        }

        for (auto it = m_hovered_entities.Begin(); it != m_hovered_entities.End();) {
            const auto ray_test_results_it = ray_test_results.FindIf([entity = *it](const RC<UIObject> &ui_object)
            {
                return ui_object->GetEntity() == entity;
            });

            if (ray_test_results_it == ray_test_results.End()) {
                if (RC<UIObject> other_ui_object = GetUIObjectForEntity(*it)) {
                    other_ui_object->SetFocusState(other_ui_object->GetFocusState() & ~UIObjectFocusState::HOVER);

                    other_ui_object->OnMouseLeave.Broadcast(MouseEvent {
                        .input_manager      = input_manager,
                        .position           = other_ui_object->TransformScreenCoordsToRelative(mouse_position),
                        .previous_position  = other_ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                        .mouse_buttons      = event.GetMouseButtons(),
                        .is_down            = false
                    });
                }

                it = m_hovered_entities.Erase(it);
            } else {
                ++it;
            }
        }

        break;
    }
    case SystemEventType::EVENT_MOUSEBUTTON_DOWN:
    {
        // project a ray into the scene and test if it hits any objects
        RayHit hit;

        UIObject *focused_object = nullptr;

        Array<RC<UIObject>> ray_test_results;

        if (TestRay(mouse_screen, ray_test_results)) {
            for (auto it = ray_test_results.Begin(); it != ray_test_results.End(); ++it) {
                if (const RC<UIObject> &ui_object = *it) {
                    if (focused_object == nullptr && ui_object->AcceptsFocus()) {
                        ui_object->Focus();

                        focused_object = ui_object.Get();
                    }

                    auto mouse_button_pressed_states_it = m_mouse_button_pressed_states.Find(ui_object->GetEntity());

                    if (mouse_button_pressed_states_it != m_mouse_button_pressed_states.End()) {
                        if ((mouse_button_pressed_states_it->second.mouse_buttons & event.GetMouseButtons()) == event.GetMouseButtons()) {
                            // already holding buttons, go to next
                            continue;
                        }

                        mouse_button_pressed_states_it->second.mouse_buttons |= event.GetMouseButtons();
                    } else {
                        mouse_button_pressed_states_it = m_mouse_button_pressed_states.Set(ui_object->GetEntity(), {
                            event.GetMouseButtons(),
                            0.0f
                        }).first;
                    }

                    ui_object->SetFocusState(ui_object->GetFocusState() | UIObjectFocusState::PRESSED);

                    event_handler_result |= ui_object->OnMouseDown.Broadcast(MouseEvent {
                        .input_manager      = input_manager,
                        .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                        .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                        .mouse_buttons      = mouse_button_pressed_states_it->second.mouse_buttons,
                        .is_down            = true
                    });

                    if (event_handler_result & UIEventHandlerResult::STOP_BUBBLING) {
                        break;
                    }
                }
            }
        }

        break;
    }
    case SystemEventType::EVENT_MOUSEBUTTON_UP:
    {
        Array<RC<UIObject>> ray_test_results;
        TestRay(mouse_screen, ray_test_results);

        for (auto &it : m_mouse_button_pressed_states) {
            const auto ray_test_results_it = ray_test_results.FindIf([entity = it.first](const RC<UIObject> &ui_object)
            {
                return ui_object->GetEntity() == entity;
            });

            if (ray_test_results_it != ray_test_results.End()) {
                // trigger click
                if (const RC<UIObject> &ui_object = *ray_test_results_it) {
                    event_handler_result |= ui_object->OnClick.Broadcast(MouseEvent {
                        .input_manager      = input_manager,
                        .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                        .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                        .mouse_buttons      = event.GetMouseButtons(),
                        .is_down            = false
                    });

                    if (event_handler_result & UIEventHandlerResult::STOP_BUBBLING) {
                        break;
                    }
                }
            }
        }

        for (auto &it : m_mouse_button_pressed_states) {
            // trigger mouse up
            if (RC<UIObject> ui_object = GetUIObjectForEntity(it.first)) {
                ui_object->SetFocusState(ui_object->GetFocusState() & ~UIObjectFocusState::PRESSED);

                event_handler_result |= ui_object->OnMouseUp.Broadcast(MouseEvent {
                    .input_manager      = input_manager,
                    .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                    .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                    .mouse_buttons      = it.second.mouse_buttons,
                    .is_down            = false
                });
            }
        }

        m_mouse_button_pressed_states.Clear();

        break;
    }
    case SystemEventType::EVENT_MOUSESCROLL:
    {
        int wheel_x;
        int wheel_y;
        event.GetMouseWheel(&wheel_x, &wheel_y);

        const Vec2u window_size = input_manager->GetWindow()->GetDimensions();

        Array<RC<UIObject>> ray_test_results;

        if (TestRay(mouse_screen, ray_test_results)) {
            UIObject *first_hit = nullptr;

            for (auto it = ray_test_results.Begin(); it != ray_test_results.End(); ++it) {
                if (const RC<UIObject> &ui_object = *it) {
                    if (first_hit) {
                        // We don't want to check the current object if it's not a child of the first hit object,
                        // since it would be behind the first hit object.
                        if (!first_hit->IsOrHasParent(ui_object)) {
                            continue;
                        }
                    } else {
                        first_hit = ui_object;
                    }

                    event_handler_result |= ui_object->OnScroll.Broadcast(MouseEvent {
                        .input_manager      = input_manager,
                        .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                        .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                        .mouse_buttons      = event.GetMouseButtons(),
                        .is_down            = false,
                        .wheel              = Vec2i { wheel_x, wheel_y }
                    });

                    if (event_handler_result & UIEventHandlerResult::STOP_BUBBLING) {
                        break;
                    }
                }
            }
        }

        break;
    }
    case SystemEventType::EVENT_KEYDOWN:
    {
        const KeyCode key_code = event.GetNormalizedKeyCode();

        RC<UIObject> ui_object = m_focused_object.Lock();

        while (ui_object != nullptr) {
            event_handler_result |= ui_object->OnKeyDown.Broadcast(KeyboardEvent {
                .input_manager  = input_manager,
                .key_code       = key_code
            });

            m_keyed_down_objects[key_code].PushBack(ui_object);

            if (event_handler_result & UIEventHandlerResult::STOP_BUBBLING) {
                break;
            }

            ui_object = RawPtrToRefCountedPtrHelper<UIObject>{}(ui_object->GetParentUIObject());
        }

        break;
    }
    case SystemEventType::EVENT_KEYUP:
    {
        const KeyCode key_code = event.GetNormalizedKeyCode();

        const auto keyed_down_objects_it = m_keyed_down_objects.Find(key_code);

        if (keyed_down_objects_it != m_keyed_down_objects.End()) {
            for (const Weak<UIObject> &weak_ui_object : keyed_down_objects_it->second) {
                if (RC<UIObject> ui_object = weak_ui_object.Lock()) {
                    ui_object->OnKeyUp.Broadcast(KeyboardEvent {
                        .input_manager  = input_manager,
                        .key_code       = key_code
                    });
                }
            }
        }

        m_keyed_down_objects.Erase(keyed_down_objects_it);

        break;
    }
    default:
        break;
    }

    return event_handler_result;
}

bool UIStage::Remove(ID<Entity> entity)
{
    if (!m_scene.IsValid()) {
        return false;
    }

    if (!GetNode()) {
        return false;
    }

    if (!m_scene->GetEntityManager()->HasEntity(entity)) {
        return false;
    }

    if (NodeProxy child_node = GetNode()->FindChildWithEntity(entity)) {
        return child_node.Remove();
    }

    return false;
}

} // namespace hyperion
