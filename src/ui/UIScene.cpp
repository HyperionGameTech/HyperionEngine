#include <ui/UIScene.hpp>
#include <util/MeshBuilder.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>
#include <system/SdlSystem.hpp>
#include <input/InputManager.hpp>
#include <Engine.hpp>
#include <Threads.hpp>

namespace hyperion::v2 {

// UIObject::UIObject()
//     : EngineComponentBase()
// {
// }

// UIObject::~UIObject()
// {
//     Teardown();
// }

// void UIObject::Init(Engine *engine)
// {
//     if (IsInitCalled()) {
//         return;
//     }

//     EngineComponentBase::Init(engine);

//     m_entity = engine->CreateHandle<Entity>(
//         engine->CreateHandle<Mesh>(MeshBuilder::Quad()),
//         Handle<Shader>(engine->shader_manager.GetShader(ShaderManager::Key::BASIC_UI)),
//         engine->CreateHandle<Material>("ui_material", Bucket::BUCKET_UI)
//     );

//     engine->InitObject(m_entity);

//     m_entity->SetTransform(m_transform);
//     m_entity->GetMaterial()->SetFaceCullMode(renderer::FaceCullMode::NONE);
//     m_entity->GetMaterial()->SetIsAlphaBlended(true);
//     m_entity->RebuildRenderableAttributes();

//     SetReady(true);

//     OnTeardown([this](...) {
//         SetReady(false);

//         m_entity.Reset();
//     });
// }

// void UIObject::SetTransform(const Transform &transform)
// {
//     m_transform = transform;

//     if (IsInitCalled()) {
//         GetEntity()->SetTransform(transform);
//     }
// }


UIScene::UIScene()
    : EngineComponentBase()
{
}

UIScene::~UIScene()
{
    Teardown();
}

void UIScene::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    m_scene = engine->CreateHandle<Scene>(
        engine->CreateHandle<Camera>(new OrthoCamera(
            2048, 2048,
            -1, 1,
            -1, 1,
            -1, 1
        ))
    );

    engine->InitObject(m_scene);

    m_scene->GetCamera()->SetDirection(Vector3(0.0f, 0.0f, -1.0f));

    // for (auto &object : m_ui_objects) {
    //     AssertThrow(engine->InitObject(object));

    //     m_scene->AddEntity(Handle<Entity>(object->GetEntity()));
    // }

    SetReady(true);

    OnTeardown([this](...) {
        SetReady(false);

        // m_ui_objects.Clear();
        m_scene.Reset();
    });
}

void UIScene::Update(Engine *engine, GameCounter::TickUnit delta)
{
    m_scene->Update(engine, delta, false);
}

bool UIScene::TestRay(const Vector2 &position, RayHit &out_first_hit)
{
    Threads::AssertOnThread(THREAD_GAME);

    // std::cout << "position : " << position << "\n";
    const Vector4 world_position = m_scene->GetCamera()->TransformScreenToWorld(position);
    // std::cout << "world_position : " << world_position << "\n";
    Vector4 ray_direction = world_position.Normalized() * -1.0f;
    // ray_direction.z = -1.0f;

    Ray ray { m_scene->GetCamera()->GetTranslation(), Vector3(ray_direction) };
    RayTestResults results;

    for (auto &it : m_scene->GetEntities()) {
        ray.TestAABB(it.second->GetWorldAABB(), results);
    }

    if (results.Any()) {
        // std::cout << "bb : " << m_ui_objects[0]->GetEntity()->GetWorldAABB() << "\n";
        out_first_hit = results.Front();
        std::cout << "hit : " << results.Front().hitpoint << "\n";

        return true;
    }

    // if (m_scene->GetRoot().Get()->TestRay(ray, results)) {
    //     out_first_hit = results.Front();

    //     return true;
    // }

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

        const Vector2 mouse_ndc(
            static_cast<Float>(mouse_x) / static_cast<Float>(input_manager->GetWindow()->width),
            static_cast<Float>(mouse_y) / static_cast<Float>(input_manager->GetWindow()->height)
        );

        if (TestRay(mouse_ndc, hit)) {
            // clicked item

            DebugLog(
                LogType::Debug,
                "Mousedown on %u\n",
                hit.id
            );

            // std::cout << ((m_ui_objects[0]->GetEntity()->GetWorldAABB().GetExtent()) * Vector3(input_manager->GetWindow()->width, input_manager->GetWindow()->height, 1.0)) << "\n";

            // std::cout << "hp : " << hit.hitpoint << "\n";
            // std::cout << "dist : " << hit.distance << "\n";

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

        const Vector2 mouse_ndc(
            static_cast<Float>(mouse_x) / static_cast<Float>(input_manager->GetWindow()->width),
            static_cast<Float>(mouse_y) / static_cast<Float>(input_manager->GetWindow()->height)
        );

        if (TestRay(mouse_ndc, hit)) {
            // clicked item

            DebugLog(
                LogType::Debug,
                "Mouse up on %u\n",
                hit.id
            );

            return true;
        }

        break;

        break;
    }
    }

    // no handle for it
    return false;
}

#if 0
bool UIScene::Add(Handle<UIObject> &&ui_object)
{
    if (!ui_object || !ui_object->GetID()) {
        return false;
    }

    auto result = m_ui_objects.Insert(ui_object->GetID(), std::move(ui_object));

    if (!result.second) {
        return false;
    }

    auto insert_result = result.first;

    if (IsInitCalled()) {
        GetEngine()->InitObject(insert_result->second);
    }

    return true;
}

bool UIScene::Add(const Handle<UIObject> &ui_object)
{
    if (!ui_object || !ui_object->GetID()) {
        return false;
    }

    auto result = m_ui_objects.Insert(ui_object->GetID(), ui_object);

    if (!result.second) {
        return false;
    }

    auto insert_result = result.first;

    if (IsInitCalled()) {
        GetEngine()->InitObject(insert_result->second);
    }

    return true;
}

bool UIScene::Remove(const UIObject::ID &id)
{
    if (!id) {
        return false;
    }

    auto it = m_ui_objects.Find(id);

    if (it == m_ui_objects.End()) {
        return false;
    }

    return m_ui_objects.Erase(it);
}
#endif

} // namespace hyperion::v2