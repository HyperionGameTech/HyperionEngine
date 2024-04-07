#include <ui/UIScene.hpp>
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

#include <Threads.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

UIScene::UIScene()
    : BasicObject(),
      m_surface_size { 1000, 1000 }
{
}

UIScene::~UIScene()
{
}

void UIScene::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    // if (const RC<Application> &application = g_engine->GetApplication()) {
    //     const auto UpdateWindowSize = [this](ApplicationWindow *window)
    //     {
    //         if (window == nullptr) {
    //             return;
    //         }

    //         const Vec2i size = Vec2i(window->GetDimensions());

    //         m_surface_size = Vec2i(size);

    //         if (m_scene.IsValid()) {
    //             m_scene->GetCamera()->SetCameraController(RC<OrthoCameraController>::Construct(
    //                 0.0f, -float(m_surface_size.x),
    //                 0.0f, float(m_surface_size.y),
    //                 float(min_depth), float(max_depth)
    //             ));
    //         }
    //     };

    //     UpdateWindowSize(application->GetCurrentWindow());
    //     m_on_current_window_changed_handler = application->OnCurrentWindowChanged.Bind(UpdateWindowSize);
    // }

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
            THREAD_GAME,
            Scene::InitInfo::SCENE_FLAGS_NON_WORLD
        }
    );

    m_scene->GetCamera()->SetCameraController(RC<OrthoCameraController>::Construct(
        0.0f, -float(m_surface_size.x),
        0.0f, float(m_surface_size.y),
        float(min_depth), float(max_depth)
    ));

    InitObject(m_scene);

    SetReady(true);
}

void UIScene::Update(GameCounter::TickUnit delta)
{
    m_scene->Update(delta);

    for (auto &it : m_mouse_held_times) {
        it.second += delta;
    }
}

bool UIScene::TestRay(const Vec2f &position, RayTestResults &out_ray_test_results)
{
    Threads::AssertOnThread(THREAD_GAME);

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

bool UIScene::OnInputEvent(
    InputManager *input_manager,
    const SystemEvent &event
)
{
    const auto GetUIObject = [this](ID<Entity> entity) -> RC<UIObject>
    {
        if (UIComponent *ui_component = m_scene->GetEntityManager()->TryGetComponent<UIComponent>(entity)) {
            return ui_component->ui_object;
        }

        return nullptr;
    };

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
        
        DebugLog(LogType::Debug, "Mouse position: %d, %d\n", mouse_x, mouse_y);
        
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
                    if (auto ui_object = GetUIObject(it.first)) {
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
                    if (auto ui_object = GetUIObject(it->id)) {
                        if (!m_hovered_entities.Insert(it->id).second) {
                            // Already hovered, skip
                            continue;
                        }

                        ui_object->SetFocusState(ui_object->GetFocusState() | UI_OBJECT_FOCUS_STATE_HOVER);

                        event_handled |= ui_object->OnMouseHover(UIMouseEventData {
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

            for (auto it = m_hovered_entities.Begin(); it != m_hovered_entities.End();) {
                const auto ray_test_results_it = ray_test_results.FindIf([entity = *it](const RayHit &hit)
                {
                    return hit.id == entity;
                });

                if (ray_test_results_it == ray_test_results.End()) {
                    if (auto other_ui_object = GetUIObject(*it)) {
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

        if (TestRay(mouse_screen, ray_test_results)) {
            for (auto it = ray_test_results.Begin(); it != ray_test_results.End(); ++it) {
                if (auto ui_object = GetUIObject(it->id)) {
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
                return hit.id == entity;
            });

            if (ray_test_results_it != ray_test_results.End()) {
                // trigger click
                if (auto ui_object = GetUIObject(ray_test_results_it->id)) {
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

        if (!event_handled) {
            for (auto &it : m_mouse_held_times) {
                // trigger mouse up
                if (auto ui_object = GetUIObject(it.first)) {
                    ui_object->SetFocusState(ui_object->GetFocusState() & ~UI_OBJECT_FOCUS_STATE_PRESSED);

                    event_handled |= ui_object->OnMouseUp(UIMouseEventData {
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

        m_mouse_held_times.Clear();

        break;
    }
    }

    return event_handled;
}

bool UIScene::Remove(ID<Entity> entity)
{
    if (!m_scene.IsValid()) {
        return false;
    }

    if (!m_scene->GetEntityManager()->HasEntity(entity)) {
        return false;
    }

    if (NodeProxy child_node = m_scene->GetRoot().Get()->FindChildWithEntity(entity)) {
        return child_node.Remove();
    }

    return false;
}

} // namespace hyperion::v2
