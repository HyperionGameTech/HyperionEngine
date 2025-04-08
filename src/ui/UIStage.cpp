/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIStage.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIText.hpp>

#include <asset/Assets.hpp>

#include <util/MeshBuilder.hpp>

#include <core/math/MathUtil.hpp>

#include <scene/World.hpp>

#include <scene/camera/OrthoCamera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>

#include <scene/Texture.hpp>

#include <scene/ecs/EntityManager.hpp>

#include <scene/ecs/components/CameraComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>

#include <input/InputManager.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

UIStage::UIStage(ThreadID owner_thread_id)
    : UIObject(UIObjectType::STAGE, owner_thread_id),
      m_surface_size { 1000, 1000 }
{
    SetName(NAME("Stage"));
    SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));

    m_camera = CreateObject<Camera>();
    m_camera->AddCameraController(MakeRefCountedPtr<OrthoCameraController>(
        0.0f, -float(m_surface_size.x),
        0.0f, float(m_surface_size.y),
        float(min_depth), float(max_depth)
    ));
    
    InitObject(m_camera);
}

UIStage::~UIStage()
{
    if (m_scene.IsValid()) {
        if (Threads::IsOnThread(m_scene->GetOwnerThreadID())) {
            m_scene->RemoveFromWorld();
        } else {
            Threads::GetThread(m_scene->GetOwnerThreadID())->GetScheduler().Enqueue([scene = m_scene]()
            {
                scene->RemoveFromWorld();
            }, TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }
}

void UIStage::SetSurfaceSize(Vec2i surface_size)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    m_surface_size = surface_size;
    
    if (m_camera.IsValid()) {
        m_camera->SetWidth(surface_size.x);
        m_camera->SetHeight(surface_size.y);

        // @FIXME: needs to remove and re-add the camera controller

        m_camera->AddCameraController(MakeRefCountedPtr<OrthoCameraController>(
            0.0f, -float(surface_size.x),
            0.0f, float(surface_size.y),
            float(min_depth), float(max_depth)
        ));
    }

    UpdateSize(true);
    UpdatePosition(true);

    SetNeedsRepaintFlag();
}

Scene *UIStage::GetScene() const
{
    if (Scene *ui_object_scene = UIObject::GetScene()) {
        return ui_object_scene;
    }

    return m_scene.Get();
}

void UIStage::SetScene(const Handle<Scene> &scene)
{
    HYP_SCOPE;

    Handle<Scene> new_scene = scene;

    if (!new_scene.IsValid()) {
        const ThreadID owner_thread_id = m_scene.IsValid() ? m_scene->GetOwnerThreadID() : ThreadID::Current();

        new_scene = CreateObject<Scene>(
            nullptr,
            owner_thread_id,
            SceneFlags::FOREGROUND | SceneFlags::UI
        );

        new_scene->SetName(CreateNameFromDynamicString(HYP_FORMAT("UIStage_{}_Scene", GetName())));
    }

    if (new_scene == m_scene) {
        return;
    }

    if (m_scene.IsValid()) {
        NodeProxy current_root_node;

        current_root_node = m_scene->GetRoot();
        AssertThrow(current_root_node.IsValid());

        current_root_node->Remove();

        new_scene->SetRoot(std::move(current_root_node));
    }

    NodeProxy camera_node = new_scene->GetRoot()->AddChild();
    camera_node->SetName("UICamera");
    
    Handle<Entity> camera_entity = new_scene->GetEntityManager()->AddEntity();
    new_scene->GetEntityManager()->AddComponent<CameraComponent>(camera_entity, CameraComponent { m_camera });

    camera_node->SetEntity(camera_entity);

    g_engine->GetWorld()->AddScene(new_scene);
    
    InitObject(new_scene);

    if (m_scene.IsValid()) {
        m_scene->RemoveFromWorld();
    }
    
    m_scene = std::move(new_scene);
}

const RC<FontAtlas> &UIStage::GetDefaultFontAtlas() const
{
    HYP_SCOPE;

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
    HYP_SCOPE;
    AssertOnOwnerThread();

    m_default_font_atlas = std::move(font_atlas);
    
    OnFontAtlasUpdate();
}

void UIStage::Init()
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    if (const RC<AppContext> &app_context = g_engine->GetAppContext()) {
        const auto UpdateSurfaceSize = [this](ApplicationWindow *window)
        {
            if (window == nullptr) {
                return;
            }

            const Vec2i size = Vec2i(window->GetDimensions());

            m_surface_size = Vec2i(size);

            if (m_camera.IsValid()) {
                m_camera->AddCameraController(MakeRefCountedPtr<OrthoCameraController>(
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

        if (font_atlas_asset.HasValue()) {
            m_default_font_atlas = font_atlas_asset->Result();
        } else {
            HYP_LOG(UI, Error, "Failed to load default font atlas! Error was: {}", font_atlas_asset.GetError().GetMessage());
        }
    }

    // Will create a new Scene
    SetScene(Handle<Scene>::empty);

    SetNodeProxy(m_scene->GetRoot());

    UIObject::Init();
}

void UIStage::AddChildUIObject(const RC<UIObject> &ui_object)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    if (!ui_object) {
        return;
    }

    UIObject::AddChildUIObject(ui_object);

    // Check if no parent stage
    if (m_stage == nullptr) {
        // Set child object stage to this
        ui_object->SetStage(this);
    }
}

void UIStage::Update_Internal(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    UIObject::Update_Internal(delta);

    for (auto &it : m_mouse_button_pressed_states) {
        it.second.held_time += delta;
    }
}

void UIStage::OnAttached_Internal(UIObject *parent)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    AssertThrow(parent != nullptr);
    AssertThrow(parent->GetNode() != nullptr);

    // Set root to be empty node proxy, now that it is attached to another object.
    m_scene->SetRoot(NodeProxy::empty);

    OnAttached();
}

void UIStage::OnRemoved_Internal()
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    // Re-set scene root to be our node proxy
    m_scene->SetRoot(m_node_proxy);
    m_scene->RemoveFromWorld();

    OnRemoved();
}

void UIStage::SetStage_Internal(UIStage *stage)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    m_stage = stage;
    
    SetNeedsRepaintFlag();
    
    // Do not update children
}

bool UIStage::TestRay(const Vec2f &position, Array<RC<UIObject>> &out_objects, EnumFlags<UIRayTestFlags> flags)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    const Vec4f world_position = Vec4f(position.x * float(GetSurfaceSize().x), position.y * float(GetSurfaceSize().y), 0.0f, 1.0f);
    const Vec3f direction { world_position.x / world_position.w, world_position.y / world_position.w, 0.0f };

    Ray ray;
    ray.position = world_position.GetXYZ() / world_position.w;
    ray.direction = direction;

    RayTestResults ray_test_results;

    for (auto [entity, ui_component, transform_component, bounding_box_component] : m_scene->GetEntityManager()->GetEntitySet<UIComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT)) {
        UIObject *ui_object = ui_component.ui_object;
        
        if (!ui_object) {
            continue;
        }

        if ((flags & UIRayTestFlags::ONLY_VISIBLE) && !ui_object->GetComputedVisibility()) {
            continue;
        }

        BoundingBox aabb = ui_object->GetAABBClamped();
        aabb.min.z = -1.0f;
        aabb.max.z = 1.0f;
     
        if (aabb.ContainsPoint(direction)) {
            RayHit hit { };
            hit.hitpoint = Vec3f { position.x, position.y, 0.0f };
            hit.distance = -float(ui_object->GetComputedDepth());
            hit.id = entity.Value();
            hit.user_data = ui_object;

            ray_test_results.AddHit(hit);
        }
    }

    out_objects.Reserve(ray_test_results.Size());

    for (const RayHit &hit : ray_test_results) {
        if (RC<UIObject> ui_object = static_cast<const UIObject *>(hit.user_data)->RefCountedPtrFromThis()) {
            out_objects.PushBack(std::move(ui_object));
        }
    }

    return out_objects.Any();
}

RC<UIObject> UIStage::GetUIObjectForEntity(ID<Entity> entity) const
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    if (UIComponent *ui_component = m_scene->GetEntityManager()->TryGetComponent<UIComponent>(entity)) {
        if (ui_component->ui_object != nullptr) {
            return ui_component->ui_object->RefCountedPtrFromThis();
        }
    }

    return nullptr;
}

void UIStage::SetFocusedObject(const RC<UIObject> &ui_object)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    if (ui_object == m_focused_object) {
        return;
    }

    RC<UIObject> current_focused_ui_object = m_focused_object.Lock();

    // Be sure to set the focused object to nullptr before calling Blur() to prevent infinite recursion
    // due to Blur() calling SetFocusedObject() again.
    m_focused_object.Reset();

    if (current_focused_ui_object != nullptr) {
        // Only blur children if 
        const bool should_blur_children = ui_object == nullptr || !ui_object->IsOrHasParent(current_focused_ui_object);

        current_focused_ui_object->Blur(should_blur_children);
    }

    m_focused_object = ui_object;

    if (UIObject *parent_stage = GetClosestParentUIObject(UIObjectType::STAGE)) {
        static_cast<UIStage *>(parent_stage)->SetFocusedObject(ui_object);
    }
}

void UIStage::ComputeActualSize(const UIObjectSize &in_size, Vec2i &out_actual_size, UpdateSizePhase phase, bool is_inner)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

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

UIEventHandlerResult UIStage::OnInputEvent(
    InputManager *input_manager,
    const SystemEvent &event
)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    UIEventHandlerResult event_handler_result = UIEventHandlerResult::OK;

    RayTestResults ray_test_results;

    const Vec2i mouse_position = input_manager->GetMousePosition();
    const Vec2i previous_mouse_position = input_manager->GetPreviousMousePosition();
    const Vec2f mouse_screen = Vec2f(mouse_position) / Vec2f(m_surface_size);

    switch (event.GetType()) {
    case SystemEventType::EVENT_MOUSEMOTION:
    {
        // check intersects with objects on mouse movement.
        // for any objects that had mouse held on them,
        // if the mouse is on them, signal mouse movement

        // project a ray into the scene and test if it hits any objects

        const EnumFlags<MouseButtonState> mouse_buttons = input_manager->GetButtonStates();

        if (mouse_buttons != MouseButtonState::NONE) { // mouse drag event
            UIEventHandlerResult mouse_drag_event_handler_result = UIEventHandlerResult::OK;

            for (const Pair<Weak<UIObject>, UIObjectPressedState> &it : m_mouse_button_pressed_states) {
                if (it.second.held_time >= 0.05f) {
                    // signal mouse drag
                    if (RC<UIObject> ui_object = it.first.Lock()) {
                        UIEventHandlerResult current_result = ui_object->OnMouseDrag(MouseEvent {
                            .input_manager      = input_manager,
                            .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                            .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                            .absolute_position  = mouse_position,
                            .mouse_buttons      = mouse_buttons
                        });

                        mouse_drag_event_handler_result |= current_result;

                        if (mouse_drag_event_handler_result & UIEventHandlerResult::STOP_BUBBLING) {
                            break;
                        }
                    }
                }
            }
        }

        Array<RC<UIObject>> ray_test_results;

        if (TestRay(mouse_screen, ray_test_results)) {
            UIObject *first_hit = nullptr;

            UIEventHandlerResult mouse_hover_event_handler_result = UIEventHandlerResult::OK;
            UIEventHandlerResult mouse_move_event_handler_result = UIEventHandlerResult::OK;

            for (auto it = ray_test_results.Begin(); it != ray_test_results.End(); ++it) {
                if (const RC<UIObject> &ui_object = *it) {
                    if (first_hit != nullptr) {
                        // We don't want to check the current object if it's not a child of the first hit object,
                        // since it would be behind the first hit object.
                        if (!first_hit->IsOrHasParent(ui_object)) {
                            continue;
                        }
                    } else {
                        first_hit = ui_object;
                    }

                    if (m_hovered_ui_objects.Contains(ui_object)) {
                        // Already hovered, trigger mouse move event instead
                        UIEventHandlerResult current_result = ui_object->OnMouseMove(MouseEvent {
                            .input_manager      = input_manager,
                            .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                            .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                            .absolute_position  = mouse_position,
                            .mouse_buttons      = mouse_buttons
                        });

                        mouse_move_event_handler_result |= current_result;

                        if (mouse_move_event_handler_result & UIEventHandlerResult::STOP_BUBBLING) {
                            break;
                        }
                    }
                }
            }

            first_hit = nullptr;

            for (auto it = ray_test_results.Begin(); it != ray_test_results.End(); ++it) {
                if (const RC<UIObject> &ui_object = *it) {
                    if (first_hit != nullptr) {
                        // We don't want to check the current object if it's not a child of the first hit object,
                        // since it would be behind the first hit object.
                        if (!first_hit->IsOrHasParent(ui_object)) {
                            continue;
                        }
                    } else {
                        first_hit = ui_object;
                    }

                    if (!m_hovered_ui_objects.Insert(ui_object).second) {
                        continue;
                    }

                    ui_object->SetFocusState(ui_object->GetFocusState() | UIObjectFocusState::HOVER);

                    UIEventHandlerResult current_result = ui_object->OnMouseHover(MouseEvent {
                        .input_manager      = input_manager,
                        .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                        .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                        .absolute_position  = mouse_position,
                        .mouse_buttons      = mouse_buttons
                    });

                    mouse_hover_event_handler_result |= current_result;

                    BoundingBoxComponent &bounding_box_component = ui_object->GetScene()->GetEntityManager()->GetComponent<BoundingBoxComponent>(ui_object->GetEntity());

                    HYP_LOG(UI, Debug, "Mouse hover on {}: {}, Text: {}, Size: {}, Inner size: {}, Node AABB: {}, Has children: {}, Size clamped: {}, Depth: {}",
                        GetClass(ui_object.GetTypeID())->GetName(),
                        ui_object->GetName(),
                        ui_object->GetText(),
                        ui_object->GetActualSize(),
                        ui_object->GetActualInnerSize(),
                        ui_object->GetNode()->GetWorldAABB().GetExtent(),
                        ui_object->HasChildUIObjects(),
                        ui_object->GetActualSizeClamped(),
                        ui_object->GetComputedDepth());

                    if (mouse_hover_event_handler_result & UIEventHandlerResult::STOP_BUBBLING) {
                        break;
                    }
                }
            }
        }

        for (auto it = m_hovered_ui_objects.Begin(); it != m_hovered_ui_objects.End();) {
            const auto ray_test_results_it = ray_test_results.FindAs(*it);

            if (ray_test_results_it == ray_test_results.End()) {
                if (RC<UIObject> ui_object = it->Lock()) {
                    ui_object->SetFocusState(ui_object->GetFocusState() & ~UIObjectFocusState::HOVER);

                    ui_object->OnMouseLeave(MouseEvent {
                        .input_manager      = input_manager,
                        .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                        .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                        .absolute_position  = mouse_position,
                        .mouse_buttons      = event.GetMouseButtons()
                    });
                } else {
                    HYP_LOG(UI, Warning, "Focused element has been destroyed");
                }

                it = m_hovered_ui_objects.Erase(it);
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

        Array<RC<UIObject>> ray_test_results;

        if (TestRay(mouse_screen, ray_test_results)) {
            UIObject *first_hit = nullptr;

            for (auto it = ray_test_results.Begin(); it != ray_test_results.End(); ++it) {
                const RC<UIObject> &ui_object = *it;

                if (!first_hit) {
                    first_hit = ui_object.Get();
                }
                
                // if (first_hit != nullptr) {
                //     // We don't want to check the current object if it's not a child of the first hit object,
                //     // since it would be behind the first hit object.
                //     if (!first_hit->IsOrHasParent(ui_object)) {
                //         continue;
                //     }
                // } else {
                //     first_hit = ui_object;
                // }

                auto mouse_button_pressed_states_it = m_mouse_button_pressed_states.FindAs(ui_object);

                if (mouse_button_pressed_states_it != m_mouse_button_pressed_states.End()) {
                    if ((mouse_button_pressed_states_it->second.mouse_buttons & event.GetMouseButtons()) == event.GetMouseButtons()) {
                        // already holding buttons, go to next
                        continue;
                    }

                    mouse_button_pressed_states_it->second.mouse_buttons |= event.GetMouseButtons();
                } else {
                    mouse_button_pressed_states_it = m_mouse_button_pressed_states.Set(ui_object, {
                        event.GetMouseButtons(),
                        0.0f
                    }).first;
                }

                ui_object->SetFocusState(ui_object->GetFocusState() | UIObjectFocusState::PRESSED);

                const UIEventHandlerResult on_mouse_down_result = ui_object->OnMouseDown(MouseEvent {
                    .input_manager      = input_manager,
                    .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                    .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                    .absolute_position  = mouse_position,
                    .mouse_buttons      = mouse_button_pressed_states_it->second.mouse_buttons
                });

                event_handler_result |= on_mouse_down_result;

                if (event_handler_result & UIEventHandlerResult::STOP_BUBBLING) {
                    break;
                }
            }
        
            if (first_hit != nullptr && first_hit->AcceptsFocus()) {
                first_hit->Focus();
            }
        }

        break;
    }
    case SystemEventType::EVENT_MOUSEBUTTON_UP:
    {
        Array<RC<UIObject>> ray_test_results;
        TestRay(mouse_screen, ray_test_results);

        for (auto it = ray_test_results.Begin(); it != ray_test_results.End(); ++it) {
            const RC<UIObject> &ui_object = *it;

            bool is_clicked = false;

            if (m_mouse_button_pressed_states.Contains(ui_object)) {
                is_clicked = true;
            }
        }

        for (auto it = ray_test_results.Begin(); it != ray_test_results.End(); ++it) {
            const RC<UIObject> &ui_object = *it;

            if (m_mouse_button_pressed_states.Contains(ui_object)) {
                const UIEventHandlerResult result = ui_object->OnClick(MouseEvent {
                    .input_manager      = input_manager,
                    .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                    .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                    .absolute_position  = mouse_position,
                    .mouse_buttons      = event.GetMouseButtons()
                });

                event_handler_result |= result;

                if (result & UIEventHandlerResult::ERR) {
                    HYP_LOG(UI, Error, "OnClick returned error: {}", result.GetMessage().GetOr("<No message>"));

                    break;
                }

                if (result & UIEventHandlerResult::STOP_BUBBLING) {
                    break;
                }
            }
        }

        for (auto &it : m_mouse_button_pressed_states) {
            // trigger mouse up
            if (RC<UIObject> ui_object = it.first.Lock()) {
                ui_object->SetFocusState(ui_object->GetFocusState() & ~UIObjectFocusState::PRESSED);

                UIEventHandlerResult current_result = ui_object->OnMouseUp(MouseEvent {
                    .input_manager      = input_manager,
                    .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                    .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                    .absolute_position  = mouse_position,
                    .mouse_buttons      = it.second.mouse_buttons
                });

                event_handler_result |= current_result;
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

        Array<RC<UIObject>> ray_test_results;

        if (TestRay(mouse_screen, ray_test_results)) {
            UIObject *first_hit = nullptr;

            for (auto it = ray_test_results.Begin(); it != ray_test_results.End(); ++it) {
                const RC<UIObject> &ui_object = *it;

                // if (first_hit) {
                //     // We don't want to check the current object if it's not a child of the first hit object,
                //     // since it would be behind the first hit object.
                //     if (!first_hit->IsOrHasParent(ui_object)) {
                //         continue;
                //     }
                // } else {
                //     first_hit = ui_object;
                // }

                UIEventHandlerResult current_result = ui_object->OnScroll(MouseEvent {
                    .input_manager      = input_manager,
                    .position           = ui_object->TransformScreenCoordsToRelative(mouse_position),
                    .previous_position  = ui_object->TransformScreenCoordsToRelative(previous_mouse_position),
                    .absolute_position  = mouse_position,
                    .mouse_buttons      = event.GetMouseButtons(),
                    .wheel              = Vec2i { wheel_x, wheel_y }
                });

                event_handler_result |= current_result;

                if (event_handler_result & UIEventHandlerResult::STOP_BUBBLING) {
                    break;
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
            HYP_LOG(UI, Debug, "Key pressed: {} on {}", uint32(key_code), ui_object->GetName());
            UIEventHandlerResult current_result = ui_object->OnKeyDown(KeyboardEvent {
                .input_manager  = input_manager,
                .key_code       = key_code
            });

            event_handler_result |= current_result;

            m_keyed_down_objects[key_code].PushBack(ui_object);

            if (event_handler_result & UIEventHandlerResult::STOP_BUBBLING) {
                break;
            }

            if (UIObject *parent = ui_object->GetParentUIObject()) {
                ui_object = parent->RefCountedPtrFromThis();
            } else {
                break;
            }
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
                    ui_object->OnKeyUp(KeyboardEvent {
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
    HYP_SCOPE;
    AssertOnOwnerThread();

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
        child_node.Remove();

        return true;
    }

    return false;
}

} // namespace hyperion
