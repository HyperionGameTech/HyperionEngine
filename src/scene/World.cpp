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

    if (m_has_scene_updates.load()) {
        PerformSceneUpdates();
    }

    for (auto &scene : m_scenes) {
        engine->InitObject(scene);
    }
    
    EngineComponentBase::Init(engine);

    SetReady(true);

    OnTeardown([this]() {
        for (auto &scene : m_scenes) {
            if (!scene) {
                continue;
            }

            scene->SetWorld(nullptr);
        }
        
        m_scenes.Clear();

        SetReady(false);
    });
}

void World::PerformSceneUpdates()
{
    std::lock_guard guard(m_scene_update_mutex);

    for (auto &scene : m_scenes_pending_removal) {
        if (scene) {
            scene->SetWorld(nullptr);
        }

        m_scenes.Erase(scene);
    }

    m_scenes_pending_removal.Clear();

    for (auto &scene : m_scenes_pending_addition) {
        if (!scene) {
            continue;
        }

        scene->SetWorld(this);

        m_scenes.Insert(std::move(scene));
    }

    m_scenes_pending_addition.Clear();

    m_has_scene_updates.store(false);
}

void World::Update(
    Engine *engine,
    GameCounter::TickUnit delta
)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    m_octree.NextVisibilityState();

    if (m_has_scene_updates.load()) {
        PerformSceneUpdates();
    }

    for (auto &scene : m_scenes) {

        scene->Update(engine, delta);
    }
}

void World::Render(
    Engine *engine,
    Frame *frame
)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    // set visibility cursor to previous Octree visibility cursor (atomic, relaxed)
    engine->render_state.visibility_cursor = m_octree.LoadPreviousVisibilityCursor();

    // for each Scene, update the TLAS
    for (auto &scene : m_scenes) {
        if (const auto &tlas = scene->GetTLAS()) {
            tlas->UpdateRender(engine, frame);
        }
    }
}

void World::AddScene(Handle<Scene> &&scene)
{
    if (!scene) {
        return;
    }

    if (IsInitCalled()) {
        GetEngine()->InitObject(scene);
    }

    std::lock_guard guard(m_scene_update_mutex);

    m_scenes_pending_addition.Insert(std::move(scene));

    m_has_scene_updates.store(true);
}

void World::RemoveScene(const Handle<Scene> &scene)
{
    if (!scene) {
        return;
    }

    std::lock_guard guard(m_scene_update_mutex);

    m_scenes_pending_removal.Insert(scene);

    m_has_scene_updates.store(true);
}

} // namespace hyperion::v2
