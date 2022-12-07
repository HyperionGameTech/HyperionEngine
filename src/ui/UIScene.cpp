#include <ui/UIScene.hpp>
#include <util/MeshBuilder.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>
#include <ui/controllers/UIController.hpp>
#include <system/SdlSystem.hpp>
#include <input/InputManager.hpp>
#include <Threads.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

UIScene::UIScene()
    : EngineComponentBase()
{
}

UIScene::~UIScene()
{
    Teardown();
}

void UIScene::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    m_scene = CreateObject<Scene>(CreateObject<Camera>());

    m_scene->GetCamera()->SetCameraController(UniquePtr<OrthoCameraController>::Construct(
        0.0f, 1.0f,
        0.0f, 1.0f,
        -1.0f, 1.0f
    ));

    InitObject(m_scene);

    m_scene->GetCamera()->SetDirection(Vector3(0.0f, 0.0f, -1.0f));

    // for (auto &object : m_ui_objects) {
    //     AssertThrow(InitObject(object));

    //     m_scene->AddEntity(Handle<Entity>(object->GetEntity()));
    // }

    SetReady(true);

    OnTeardown([this](...) {
        SetReady(false);

        // m_ui_objects.Clear();
        m_scene.Reset();
    });
}

void UIScene::Update(GameCounter::TickUnit delta)
{
    m_scene->Update(delta);

    for (auto &it : m_mouse_held_times) {
        it.second += delta;
    }
}

bool UIScene::TestRay(const Vector2 &position, RayHit &out_first_hit)
{
    Threads::AssertOnThread(THREAD_GAME);

    const Vector4 world_position = m_scene->GetCamera()->TransformScreenToWorld(position);
    const Vector3 direction = Vector3(Vector2(world_position.x, world_position.y) / world_position.w, 0.0f);
    
    RayTestResults results;

    for (auto &it : m_scene->GetEntities()) {
        if (it.second->GetWorldAABB().ContainsPoint(direction)) {
            RayHit hit { };
            hit.hitpoint = Vector3(position, 0.0f);
            hit.distance = m_scene->GetCamera()->TransformWorldToNDC(it.second->GetTranslation()).z;
            hit.id = it.first.value;

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

        const auto extent = input_manager->GetWindow()->GetExtent();

        const Vector2 mouse_screen(
            Float(mouse_x) / Float(extent.width),
            Float(mouse_y) / Float(extent.height)
        );

        if (TestRay(mouse_screen, hit)) {
            UIEvent ui_event;
            ui_event.type = UIEvent::Type::MOUSE_DRAG;
            ui_event.original_event = &event;
            ui_event.mouse_position = mouse_screen;

            if (const Handle<Entity> &entity = m_scene->FindEntityWithID(ID<Entity>(hit.id))) {
                auto it = m_mouse_held_times.Find(entity->GetID());

                if (it != m_mouse_held_times.End()) {
                    if (it->second >= 0.25f) {
                        // signal mouse drag
                        for (auto &it : entity->GetControllers()) {
                            if (UIController *ui_controller = dynamic_cast<UIController *>(it.second.Get())) {
                                ui_controller->OnEvent(ui_event);

                                return true;
                            }
                        }
                    }
                } else {
                    ui_event.type = UIEvent::Type::MOUSE_HOVER;

                    for (auto &it : entity->GetControllers()) {
                        if (UIController *ui_controller = dynamic_cast<UIController *>(it.second.Get())) {
                            ui_controller->OnEvent(ui_event);

                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }
    case SystemEventType::EVENT_MOUSEBUTTON_DOWN: {
        // project a ray into the scene and test if it hits any objects
        RayHit hit;

        const auto &mouse_position = input_manager->GetMousePosition();
        const auto mouse_x = mouse_position.x.load();
        const auto mouse_y = mouse_position.y.load();

        const auto extent = input_manager->GetWindow()->GetExtent();

        const Vector2 mouse_screen(
            Float(mouse_x) / Float(extent.width),
            Float(mouse_y) / Float(extent.height)
        );

        if (TestRay(mouse_screen, hit)) {
            UIEvent ui_event;
            ui_event.type = UIEvent::Type::MOUSE_DOWN;
            ui_event.original_event = &event;
            ui_event.mouse_position = mouse_screen;

            if (const Handle<Entity> &entity = m_scene->FindEntityWithID(ID<Entity>(hit.id))) {
                m_mouse_held_times.Insert(entity->GetID(), 0.0f);

                for (auto &it : entity->GetControllers()) {
                    if (UIController *ui_controller = dynamic_cast<UIController *>(it.second.Get())) {
                        ui_controller->OnEvent(ui_event);
                    }
                }
            }

            return true;
        }

        break;
    }
    case SystemEventType::EVENT_MOUSEBUTTON_UP: {
       

        const auto &mouse_position = input_manager->GetMousePosition();
        const auto mouse_x = mouse_position.x.load();
        const auto mouse_y = mouse_position.y.load();

        const auto extent = input_manager->GetWindow()->GetExtent();

        const Vector2 mouse_screen(
            Float(mouse_x) / Float(extent.width),
            Float(mouse_y) / Float(extent.height)
        );

        bool result = false;

        for (auto &it : m_mouse_held_times) {
            UIEvent ui_event;
            ui_event.type = UIEvent::Type::MOUSE_UP;
            ui_event.original_event = &event;
            ui_event.mouse_position = mouse_screen;

            if (const Handle<Entity> &entity = m_scene->FindEntityWithID(it.first)) {
                for (auto &it : entity->GetControllers()) {
                    if (UIController *ui_controller = dynamic_cast<UIController *>(it.second.Get())) {
                        RayHit hit;

                        if (TestRay(mouse_screen, hit)) {
                            UIEvent click_event;
                            click_event.type = UIEvent::Type::CLICK;
                            click_event.original_event = &event;
                            click_event.mouse_position = mouse_screen;

                            ui_controller->OnEvent(click_event);
                        }

                        ui_controller->OnEvent(ui_event);

                        result = true;
                    }
                }
            }
        }

        m_mouse_held_times.Clear();

        return result;
    }
    }

    // no handle for it
    return false;
}

} // namespace hyperion::v2