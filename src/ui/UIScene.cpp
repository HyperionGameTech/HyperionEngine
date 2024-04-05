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
      m_surface_size { 800, 450 }
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

bool UIScene::TestRay(const Vec2f &position, RayHit &out_first_hit)
{
    Threads::AssertOnThread(THREAD_GAME);

    const Vec4f world_position = m_scene->GetCamera()->TransformScreenToWorld(position);
    const Vec3f direction { world_position.x / world_position.w, world_position.y / world_position.w, 0.0f };

    Ray ray;
    ray.position = world_position.GetXYZ() / world_position.w;
    ray.direction = direction;
    
    RayTestResults results;

    for (auto [entity_id, ui_component, transform_component, bounding_box_component] : m_scene->GetEntityManager()->GetEntitySet<UIComponent, TransformComponent, BoundingBoxComponent>()) {
        // DebugLog(
        //     LogType::Debug,
        //     "Testing AABB [%f, %f, %f, %f, %f, %f] against point [%f, %f, %f]\n",
        //     bounding_box_component.world_aabb.min.x,
        //     bounding_box_component.world_aabb.min.y,
        //     bounding_box_component.world_aabb.min.z,
        //     bounding_box_component.world_aabb.max.x,
        //     bounding_box_component.world_aabb.max.y,
        //     bounding_box_component.world_aabb.max.z,
        //     direction.x,
        //     direction.y,
        //     direction.z
        // );

        BoundingBox aabb(bounding_box_component.world_aabb);
        aabb.min.z = -1.0f;
        aabb.max.z = 1.0f;
        
        if (aabb.ContainsPoint(direction)) {
            RayHit hit { };
            hit.hitpoint = Vec3f { position.x, position.y, 0.0f };
            hit.distance = m_scene->GetCamera()->TransformWorldToNDC(transform_component.transform.GetTranslation()).z;
            hit.id = entity_id.value;

            results.AddHit(hit);
        }
    }

    if (results.Any()) {
        out_first_hit = results.Front();
        
        return true;
    }

    return false;
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

    switch (event.GetType()) {
    case SystemEventType::EVENT_MOUSEMOTION: {
        // check intersects with objects on mouse movement.
        // for any objects that had mouse held on them,
        // if the mouse is on them, signal mouse movement

        // project a ray into the scene and test if it hits any objects
        RayHit hit;

        const auto &mouse_position = input_manager->GetMousePosition();
        const auto mouse_x = mouse_position.x.load();
        const auto mouse_y = mouse_position.y.load();
        
        
        const bool is_mouse_button_down = input_manager->IsButtonDown(event.GetMouseButton());

        const auto extent = input_manager->GetWindow()->GetExtent();

        const Vec2f mouse_screen(
            float(mouse_x) / float(extent.width),
            float(mouse_y) / float(extent.height)
        );

        bool event_handled = false;

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
                    }
                }
            }
        } else {
            if (TestRay(mouse_screen, hit)) {
                // signal mouse hover
                if (auto ui_object = GetUIObject(hit.id)) {
                    event_handled |= ui_object->OnMouseHover(UIMouseEventData {
                        .position   = mouse_screen,
                        .button     = event.GetMouseButton(),
                        .is_down    = false
                    });
                }
            }
        }

        return event_handled;
    }
    case SystemEventType::EVENT_MOUSEBUTTON_DOWN: {
        // project a ray into the scene and test if it hits any objects
        RayHit hit;

        const auto &mouse_position = input_manager->GetMousePosition();
        const auto mouse_x = mouse_position.x.load();
        const auto mouse_y = mouse_position.y.load();

        const auto extent = input_manager->GetWindow()->GetExtent();

        const Vec2f mouse_screen(
            float(mouse_x) / float(extent.width),
            float(mouse_y) / float(extent.height)
        );

        if (TestRay(mouse_screen, hit)) {
            m_mouse_held_times.Insert(hit.id, 0.0f);

            // trigger mouse down
            if (auto ui_object = GetUIObject(hit.id)) {
                return ui_object->OnMouseDown(UIMouseEventData {
                    .position   = mouse_screen,
                    .button     = event.GetMouseButton(),
                    .is_down    = true
                });
            }
        }

        break;
    }
    case SystemEventType::EVENT_MOUSEBUTTON_UP: {
        const auto &mouse_position = input_manager->GetMousePosition();
        const auto mouse_x = mouse_position.x.load();
        const auto mouse_y = mouse_position.y.load();

        const auto extent = input_manager->GetWindow()->GetExtent();

        const Vec2f mouse_screen(
            float(mouse_x) / float(extent.width),
            float(mouse_y) / float(extent.height)
        );

        bool result = false;

        for (auto &it : m_mouse_held_times) {
            RayHit hit;

            if (TestRay(mouse_screen, hit)) {
                // trigger click
                if (auto ui_object = GetUIObject(hit.id)) {
                    result |= ui_object->OnClick(UIMouseEventData {
                        .position   = mouse_screen,
                        .button     = event.GetMouseButton(),
                        .is_down    = false
                    });
                }
            }

            // trigger mouse up
            if (auto ui_object = GetUIObject(it.first)) {
                result |= ui_object->OnMouseUp(UIMouseEventData {
                    .position   = mouse_screen,
                    .button     = event.GetMouseButton(),
                    .is_down    = false
                });
            }

            if (result) {
                break;
            }
        }

        m_mouse_held_times.Clear();

        return result;
    }
    }

    // no handle for it
    return false;
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
