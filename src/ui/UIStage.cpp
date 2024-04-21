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

#include <system/Application.hpp>

#include <input/InputManager.hpp>

#include <core/threading/Threads.hpp>
#include <Engine.hpp>

namespace hyperion {

UIStage::UIStage()
    : UIObject(),
      m_surface_size { 1000, 1000 }
{
    SetName(HYP_NAME(Stage));
}

UIStage::~UIStage()
{
}

void UIStage::Init()
{
    if (const RC<Application> &application = g_engine->GetApplication()) {
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

        UpdateWindowSize(application->GetCurrentWindow());
        m_on_current_window_changed_handler = application->OnCurrentWindowChanged.Bind(UpdateWindowSize);
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

bool UIStage::TestRay(const Vec2f &position, RayTestResults &out_ray_test_results)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    out_ray_test_results.Clear();

    const Vec4f world_position = m_scene->GetCamera()->TransformScreenToWorld(position);
    const Vec3f direction { world_position.x / world_position.w, world_position.y / world_position.w, 0.0f };

    Ray ray;
    ray.position = world_position.GetXYZ() / world_position.w;
    ray.direction = direction;

    for (auto [entity_id, ui_component, transform_component, bounding_box_component] : m_scene->GetEntityManager()->GetEntitySet<UIComponent, TransformComponent, BoundingBoxComponent>()) {
        if (!ui_component.ui_object) {
            continue;
        }

        BoundingBox aabb(bounding_box_component.world_aabb);
        aabb.min.z = -1.0f;
        aabb.max.z = 1.0f;
        
        if (aabb.ContainsPoint(direction)) {
            RayHit hit { };
            hit.hitpoint = Vec3f { position.x, position.y, 0.0f };
            hit.distance = m_scene->GetCamera()->TransformWorldToNDC(transform_component.transform.GetTranslation()).z;
            hit.id = entity_id.value;

            out_ray_test_results.AddHit(hit);
        }
    }

    return out_ray_test_results.Any();
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
    RC<UIObject> current_focused_ui_object = m_focused_object.Lock();

    if (current_focused_ui_object != nullptr) {
        if (current_focused_ui_object == ui_object) {
            return;
        }

        current_focused_ui_object->Blur();
    }

    if (!ui_object) {
        m_focused_object.Reset();

        return;
    }

    m_focused_object = ui_object;
}

bool UIStage::OnInputEvent(
    InputManager *input_manager,
    const SystemEvent &event
)
{
    bool event_handled = false;
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
                        event_handled |= ui_object->OnMouseDrag(UIMouseEventData {
                            .position   = mouse_screen,
                            .button     = event.GetMouseButton(),
                            .is_down    = true
                        });

                        if (event_handled) {
                            break;
                        }
                    }
                }
            }
        } else {
            if (TestRay(mouse_screen, ray_test_results)) {
                for (auto it = ray_test_results.Begin(); it != ray_test_results.End(); ++it) {
                    if (auto ui_object = GetUIObjectForEntity(it->id)) {
                        if (!m_hovered_entities.Insert(it->id).second) {
                            // Already hovered, skip
                            continue;
                        }

                        ui_object->SetFocusState(ui_object->GetFocusState() | UI_OBJECT_FOCUS_STATE_HOVER);

                        DebugLog(LogType::Debug,
                            "Mouse hover on object: %s, DrawableLayer: %u\tDepth: %d\t%z: %f\n",
                            ui_object->GetName().LookupString(),
                            ui_object->GetDrawableLayer().layer_index,
                            ui_object->GetDepth(),
                            ui_object->GetNode()->GetWorldTranslation().z
                        );

                        event_handled |= ui_object->OnMouseHover(UIMouseEventData {
                            .position   = mouse_screen,
                            .button     = event.GetMouseButton(),
                            .is_down    = false
                        });

                        // if (event_handled) {
                        //     break;
                        // }
                    }
                }
            }

            for (auto it = m_hovered_entities.Begin(); it != m_hovered_entities.End();) {
                const auto ray_test_results_it = ray_test_results.FindIf([entity = *it](const RayHit &hit)
                {
                    return hit.id == entity.Value();
                });

                if (ray_test_results_it == ray_test_results.End()) {
                    if (auto other_ui_object = GetUIObjectForEntity(*it)) {
                        other_ui_object->SetFocusState(other_ui_object->GetFocusState() & ~UI_OBJECT_FOCUS_STATE_HOVER);

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

        if (TestRay(mouse_screen, ray_test_results)) {
            for (auto it = ray_test_results.Begin(); it != ray_test_results.End(); ++it) {
                if (auto ui_object = GetUIObjectForEntity(it->id)) {
                    if (focused_object == nullptr && ui_object->AcceptsFocus()) {
                        ui_object->Focus();

                        focused_object = ui_object.Get();
                    }

                    m_mouse_held_times.Insert(it->id, 0.0f);

                    ui_object->SetFocusState(ui_object->GetFocusState() | UI_OBJECT_FOCUS_STATE_PRESSED);

                    event_handled |= ui_object->OnMouseDown(UIMouseEventData {
                        .position   = mouse_screen,
                        .button     = event.GetMouseButton(),
                        .is_down    = true
                    });

                    if (event_handled) {
                        break;
                    }
                }
            }
        }

        if (focused_object == nullptr) {
            SetFocusedObject(nullptr);
        }

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

        TestRay(mouse_screen, ray_test_results);

        for (auto &it : m_mouse_held_times) {
            const auto ray_test_results_it = ray_test_results.FindIf([entity = it.first](const RayHit &hit)
            {
                return hit.id == entity.Value();
            });

            if (ray_test_results_it != ray_test_results.End()) {
                // trigger click
                if (auto ui_object = GetUIObjectForEntity(ray_test_results_it->id)) {
                    event_handled |= ui_object->OnClick(UIMouseEventData {
                        .position   = mouse_screen,
                        .button     = event.GetMouseButton(),
                        .is_down    = false
                    });

                    if (event_handled) {
                        break;
                    }
                }
            }
        }

        for (auto &it : m_mouse_held_times) {
            // trigger mouse up
            if (auto ui_object = GetUIObjectForEntity(it.first)) {
                ui_object->SetFocusState(ui_object->GetFocusState() & ~UI_OBJECT_FOCUS_STATE_PRESSED);

                event_handled |= ui_object->OnMouseUp(UIMouseEventData {
                    .position   = mouse_screen,
                    .button     = event.GetMouseButton(),
                    .is_down    = false
                });
            }
        }

        m_mouse_held_times.Clear();

        break;
    }
    default:
        break;
    }

    return event_handled;
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
