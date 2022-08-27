#include "World.hpp"
#include <Engine.hpp>
#include <Threads.hpp>

namespace hyperion::v2 {

World::World()
    : EngineComponentBase(),
      m_octree(BoundingBox(Vector3(-250.0f), Vector3(250.0f)))
{
}

World::~World()
{
    Teardown();
}
    
void World::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }
    
    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ANY, [this](...) {
        auto *engine = GetEngine();

        SetReady(true);

        OnTeardown([this]() {
            auto *engine = GetEngine();

            for (auto &it : m_scenes) {
                auto &scene = it.second;

                if (scene == nullptr) {
                    continue;
                }

                scene->SetWorld(nullptr);
            }
            
            m_scenes.Clear();

            SetReady(false);
        });
    }));
}

void World::Update(
    Engine *engine,
    GameCounter::TickUnit delta
)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    m_octree.NextVisibilityState();

    for (auto &it : m_scenes) {
        auto &scene = it.second;
        AssertThrow(scene != nullptr);

        scene->Update(engine, delta);
    }
}

void World::AddScene(Handle<Scene> &&scene)
{
    Threads::AssertOnThread(THREAD_GAME);

    if (scene == nullptr) {
        return;
    }

    if (IsInitCalled()) {
        GetEngine()->InitObject(scene);
    }

    scene->SetWorld(this);

    const auto scene_id = scene->GetId();
    m_scenes.Insert(scene_id, std::move(scene));
}

void World::RemoveScene(Scene::ID scene_id)
{
    Threads::AssertOnThread(THREAD_GAME);

    auto it = m_scenes.Find(scene_id);

    if (it == m_scenes.End()) {
        return;
    }

    auto &scene = it->second;

    if (scene != nullptr) {
        scene->SetWorld(nullptr);
    }

    m_scenes.Erase(it);
}

} // namespace hyperion::v2
