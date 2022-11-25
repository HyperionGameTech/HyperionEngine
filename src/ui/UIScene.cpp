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

    EngineComponentBase::Init(Engine::Get());

    m_scene = Engine::Get()->CreateHandle<Scene>(
        Engine::Get()->CreateHandle<Camera>(new OrthoCamera(
            2048, 2048,
            -1, 1,
            -1, 1,
            -1, 1
        ))
    );

    Engine::Get()->InitObject(m_scene);

    m_scene->GetCamera()->SetDirection(Vector3(0.0f, 0.0f, -1.0f));

    // for (auto &object : m_ui_objects) {
    //     AssertThrow(Engine::Get()->InitObject(object));

    //     m_scene->AddEntity(Handle<Entity>(object->GetEntity()));
    // }

    SetReady(true);

    OnTeardown([this](...) {
        SetReady(false);

        // m_ui_objects.Clear();
        m_scene.Reset();
    });
}

void UIScene::Update( GameCounter::TickUnit delta)
{
    m_scene->Update(Engine::Get(), delta, false);
}

bool UIScene::TestRay(const Vector2 &position, RayHit &out_first_hit)
{
    Threads::AssertOnThread(THREAD_GAME);

    const Vector4 world_position = m_scene->GetCamera()->TransformScreenToWorld(position);
    const Vector4 ray_direction = world_position.Normalized() * -1.0f;
    
    RayTestResults results;

    for (auto &it : m_scene->GetEntities()) {
        if (it.second->GetWorldAABB().ContainsPoint(Vector3(ray_direction.x, ray_direction.y, 0.0f))) {
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

// void UIScene::Add(Handle<UIObject> &&object)
// {
//     Threads::AssertOnThread(THREAD_GAME);

//     if (!object) {
//         return;
//     }

//     if (IsInitCalled()) {
//         GetEngine()->InitObject(object);

//         m_scene->AddEntity(Handle<Entity>(object->GetEntity()));
//     }

//     m_ui_objects.PushBack(std::move(object));
// }

bool UIScene::OnInputEvent(
    InputManager *input_manager,
    const SystemEvent &event
)
{
    switch (event.GetType()) {
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

            if (const Handle<Entity> &entity = m_scene->FindEntityWithID(Entity::ID(hit.id))) {
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
            ui_event.type = UIEvent::Type::MOUSE_UP;
            ui_event.original_event = &event;

            if (const Handle<Entity> &entity = m_scene->FindEntityWithID(Entity::ID(hit.id))) {
                for (auto &it : entity->GetControllers()) {
                    if (UIController *ui_controller = dynamic_cast<UIController *>(it.second.Get())) {
                        ui_controller->OnEvent(ui_event);
                    }
                }
            }

            return true;
        }

        break;

        break;
    }
    }

    // no handle for it
    return false;
}

} // namespace hyperion::v2