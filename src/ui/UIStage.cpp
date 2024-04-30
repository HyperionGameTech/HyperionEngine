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
#include <core/threading/Threads.hpp>

#include <input/InputManager.hpp>

#include <Engine.hpp>

namespace hyperion {

UIStage::UIStage()
    : UIObject(UOT_STAGE),
      m_surface_size { 1000, 1000 }
{
    SetName(HYP_NAME(Stage));
}

UIStage::~UIStage()
{
}

void UIStage::Init()
{
    if (const RC<AppContext> &app_context = g_engine->GetAppContext()) {
        const auto UpdateWindowSize = [this](ApplicationWindow *window)
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

        UpdateWindowSize(app_context->GetCurrentWindow());
        m_on_current_window_changed_handler = app_context->OnCurrentWindowChanged.Bind(UpdateWindowSize);
    }

    if (!m_default_font_atlas) {
        LoaderResult loader_result;
        RC<FontAtlas> font_atlas = g_asset_manager->Load<FontAtlas>("fonts/default.json", loader_result);

        if (font_atlas != nullptr) {
            m_default_font_atlas = std::move(font_atlas);
        } else {
            DebugLog(LogType::Error, "Failed to load default font atlas! Error was: %s\n", loader_result.message.Data());
        }
    }

    m_scene = CreateObject<Scene>(
        CreateObject<Camera>(),
        Scene::InitInfo
        {
            ThreadName::THREAD_GAME,
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

    // @FIXME: Circular reference
    m_scene->GetEntityManager()->AddComponent(m_scene->GetRoot()->GetEntity(), UIComponent { RefCountedPtrFromThis() });

    m_scene->GetRoot()->LockTransform();

    SetNodeProxy(m_scene->GetRoot());

    SetSize(UIObjectSize({ m_scene->GetCamera()->GetWidth(), UIObjectSize::PIXEL }, { m_scene->GetCamera()->GetHeight(), UIObjectSize::PIXEL }));
    m_actual_size = { m_scene->GetCamera()->GetWidth(), m_scene->GetCamera()->GetHeight() };

    UIObject::Init();
}

void UIStage::Update(GameCounter::TickUnit delta)
{
    m_scene->Update(delta);

    for (auto &it : m_mouse_held_times) {
        it.second += delta;
    }
}

bool UIStage::TestRay(const Vec2f &position, Array<RC<UIObject>> &out_objects)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    const Vec4f world_position = m_scene->GetCamera()->TransformScreenToWorld(position);
    const Vec3f direction { world_position.x / world_position.w, world_position.y / world_position.w, 0.0f };

    Ray ray;
    ray.position = world_position.GetXYZ() / world_position.w;
    ray.direction = direction;

    CollectObjects([&out_objects, &direction, &position](const RC<UIObject> &ui_object)
    {
        BoundingBox aabb(ui_object->GetWorldAABB());
        aabb.min.z = -1.0f;
        aabb.max.z = 1.0f;
        
        if (aabb.ContainsPoint(direction)) {
            out_objects.PushBack(ui_object);
        }
    });

    // for (auto [entity_id, ui_component, transform_component, bounding_box_component] : m_scene->GetEntityManager()->GetEntitySet<UIComponent, TransformComponent, BoundingBoxComponent>()) {
    //     if (!ui_component.ui_object) {
    //         continue;
    //     }

    //     BoundingBox aabb(bounding_box_component.world_aabb);
    //     aabb.min.z = -1.0f;
    //     aabb.max.z = 1.0f;
        
    //     if (aabb.ContainsPoint(direction)) {
    //         RayHit hit { };
    //         hit.hitpoint = Vec3f { position.x, position.y, 0.0f };
    //         hit.distance = -float(ui_component.ui_object->GetComputedDepth());
    //         hit.id = entity_id.value;

    //         out_ray_test_results.AddHit(hit);
    //     }
    // }

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
    if (ui_object) {
        DebugLog(LogType::Debug, "SetFocusedObject: %s\n", *ui_object->GetName());
    } else {
        DebugLog(LogType::Debug, "SetFocusedObject: nullptr\n");
    }

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

UIEventHandlerResult UIStage::OnInputEvent(
    InputManager *input_manager,
    const SystemEvent &event
)
{
    UIEventHandlerResult event_handler_result = UEHR_OK;

    RayTestResults ray_test_results;

    switch (event.GetType()) {
    case SystemEventType::EVENT_MOUSEMOTION: {
        // check intersects with objects on mouse movement.
        // for any objects that had mouse held on them,
        // if the mouse is on them, signal mouse movement

        // project a ray into the scene and test if it hits any objects

        const auto &mouse_position = input_manager->GetMousePosition();
        const auto mouse_x = mouse_position.x.load();
        const auto mouse_y = mouse_position.y.load();
        
        const bool is_mouse_button_down = input_manager->IsButtonDown(event.GetMouseButton());

        const Vec2u window_size = input_manager->GetWindow()->GetDimensions();

        const Vec2f mouse_screen(
            float(mouse_x) / float(window_size.x),
            float(mouse_y) / float(window_size.y)
        );

        if (is_mouse_button_down) { // mouse drag event
            for (auto &it : m_mouse_held_times) {
                if (it.second >= 0.05f) {
                    // signal mouse drag
                    if (auto ui_object = GetUIObjectForEntity(it.first)) {
                        event_handler_result |= ui_object->OnMouseDrag(UIMouseEventData {
                            .position   = mouse_screen,
                            .button     = event.GetMouseButton(),
                            .is_down    = true
                        });

                        if (event_handler_result & UEHR_STOP_BUBBLING) {
                            break;
                        }
                    }
                }
            }
        } else {
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

                        if (!m_hovered_entities.Insert(ui_object->GetEntity()).second) {
                            // Already hovered, skip
                            continue;
                        }

                        ui_object->SetFocusState(ui_object->GetFocusState() | UOFS_HOVER);

                        DebugLog(LogType::Debug,
                            "Mouse hover on object: %s, DrawableLayer: %u\tDepth: %d\t%z: %f\n",
                            ui_object->GetName().LookupString(),
                            ui_object->GetDrawableLayer().layer_index,
                            ui_object->GetDepth(),
                            ui_object->GetNode()->GetWorldTranslation().z
                        );

                        event_handler_result |= ui_object->OnMouseHover(UIMouseEventData {
                            .position   = mouse_screen,
                            .button     = event.GetMouseButton(),
                            .is_down    = false
                        });

                        if (event_handler_result & UEHR_STOP_BUBBLING) {
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
                    if (auto other_ui_object = GetUIObjectForEntity(*it)) {
                        other_ui_object->SetFocusState(other_ui_object->GetFocusState() & ~UOFS_HOVER);

                        other_ui_object->OnMouseLeave(UIMouseEventData {
                            .position   = mouse_screen,
                            .button     = event.GetMouseButton(),
                            .is_down    = false
                        });
                    }

                    it = m_hovered_entities.Erase(it);
                } else {
                    ++it;
                }
            }
        }

        break;
    }
    case SystemEventType::EVENT_MOUSEBUTTON_DOWN: {
        // project a ray into the scene and test if it hits any objects
        RayHit hit;

        const auto &mouse_position = input_manager->GetMousePosition();
        const auto mouse_x = mouse_position.x.load();
        const auto mouse_y = mouse_position.y.load();

        const Vec2u window_size = input_manager->GetWindow()->GetDimensions();

        const Vec2f mouse_screen(
            float(mouse_x) / float(window_size.x),
            float(mouse_y) / float(window_size.y)
        );

        UIObject *focused_object = nullptr;

        Array<RC<UIObject>> ray_test_results;

        if (TestRay(mouse_screen, ray_test_results)) {
            for (auto it = ray_test_results.Begin(); it != ray_test_results.End(); ++it) {
                if (const RC<UIObject> &ui_object = *it) {
                    if (focused_object == nullptr && ui_object->AcceptsFocus()) {
                        ui_object->Focus();

                        focused_object = ui_object.Get();
                    }

                    m_mouse_held_times.Set(ui_object->GetEntity(), 0.0f);

                    ui_object->SetFocusState(ui_object->GetFocusState() | UOFS_PRESSED);

                    event_handler_result |= ui_object->OnMouseDown(UIMouseEventData {
                        .position   = mouse_screen,
                        .button     = event.GetMouseButton(),
                        .is_down    = true
                    });

                    if (event_handler_result & UEHR_STOP_BUBBLING) {
                        break;
                    }
                }
            }
        }

        // if (focused_object == nullptr) {
        //     SetFocusedObject(nullptr);
        // }

        break;
    }
    case SystemEventType::EVENT_MOUSEBUTTON_UP: {
        const auto &mouse_position = input_manager->GetMousePosition();
        const auto mouse_x = mouse_position.x.load();
        const auto mouse_y = mouse_position.y.load();

        const Vec2u window_size = input_manager->GetWindow()->GetDimensions();

        const Vec2f mouse_screen(
            float(mouse_x) / float(window_size.x),
            float(mouse_y) / float(window_size.y)
        );

        Array<RC<UIObject>> ray_test_results;
        TestRay(mouse_screen, ray_test_results);

        for (auto &it : m_mouse_held_times) {
            const auto ray_test_results_it = ray_test_results.FindIf([entity = it.first](const RC<UIObject> &ui_object)
            {
                return ui_object->GetEntity() == entity;
            });

            if (ray_test_results_it != ray_test_results.End()) {
                // trigger click
                if (const RC<UIObject> &ui_object = *ray_test_results_it) {
                    event_handler_result |= ui_object->OnClick(UIMouseEventData {
                        .position   = mouse_screen,
                        .button     = event.GetMouseButton(),
                        .is_down    = false
                    });

                    if (event_handler_result & UEHR_STOP_BUBBLING) {
                        break;
                    }
                }
            }
        }

        for (auto &it : m_mouse_held_times) {
            // trigger mouse up
            if (auto ui_object = GetUIObjectForEntity(it.first)) {
                ui_object->SetFocusState(ui_object->GetFocusState() & ~UOFS_PRESSED);

                event_handler_result |= ui_object->OnMouseUp(UIMouseEventData {
                    .position   = mouse_screen,
                    .button     = event.GetMouseButton(),
                    .is_down    = false
                });
            }
        }

        m_mouse_held_times.Clear();

        break;
    }
    case SystemEventType::EVENT_MOUSESCROLL: {
        const auto &mouse_position = input_manager->GetMousePosition();
        const auto mouse_x = mouse_position.x.load();
        const auto mouse_y = mouse_position.y.load();

        int wheel_x;
        int wheel_y;
        event.GetMouseWheel(&wheel_x, &wheel_y);

        const Vec2u window_size = input_manager->GetWindow()->GetDimensions();

        const Vec2f mouse_screen(
            float(mouse_x) / float(window_size.x),
            float(mouse_y) / float(window_size.y)
        );

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

                        event_handler_result |= ui_object->OnScroll(UIMouseEventData {
                            .position   = mouse_screen,
                            .button     = event.GetMouseButton(),
                            .is_down    = input_manager->IsButtonDown(event.GetMouseButton()),
                            .wheel      = Vec2i { wheel_x, wheel_y }
                        });

                        if (event_handler_result & UEHR_STOP_BUBBLING) {
                            break;
                        }
                    }
                }
            }

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
