#include <ui/UIScene.hpp>
#include <builders/MeshBuilder.hpp>
#include <camera/OrthoCamera.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

UIObject::UIObject()
    : EngineComponentBase()
{
}

UIObject::~UIObject()
{
    Teardown();
}

void UIObject::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    m_entity = engine->CreateHandle<Entity>(
        engine->CreateHandle<Mesh>(MeshBuilder::Quad()),
        Handle<Shader>(engine->shader_manager.GetShader(ShaderManager::Key::BASIC_UI)),
        engine->CreateHandle<Material>("ui_material", Bucket::BUCKET_UI)
    );

    engine->InitObject(m_entity);

    m_entity->SetTransform(m_transform);
    m_entity->RebuildRenderableAttributes();

    SetReady(true);

    OnTeardown([this](...) {
        SetReady(false);

        m_entity.Reset();
    });
}

void UIObject::SetTransform(const Transform &transform)
{
    m_transform = transform;

    if (IsInitCalled()) {
        GetEntity()->SetTransform(transform);
    }
}


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
            1024, 1024,
            -1, 1,
            -1, 1,
            -1, 1
        ))
    );

    engine->InitObject(m_scene);

    for (auto &object : m_ui_objects) {
        AssertThrow(engine->InitObject(object));

        m_scene->AddEntity(Handle<Entity>(object->GetEntity()));
    }

    SetReady(true);

    OnTeardown([this](...) {
        SetReady(false);

        m_ui_objects.Clear();
        m_scene.Reset();
    });

    // for (auto &ui_object : m_ui_objects) {
    //     engine->InitObject(ui_object);
    // }
}

void UIScene::Update(Engine *engine, GameCounter::TickUnit delta)
{
    m_scene->Update(engine, delta, false);
}

void UIScene::Add(Handle<UIObject> &&object)
{
    if (!object) {
        return;
    }

    if (IsInitCalled()) {
        GetEngine()->InitObject(object);

        m_scene->AddEntity(Handle<Entity>(object->GetEntity()));
    }

    m_ui_objects.PushBack(std::move(object));
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