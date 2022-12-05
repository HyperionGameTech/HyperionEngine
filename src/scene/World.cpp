#include "World.hpp"
#include <Engine.hpp>
#include <Threads.hpp>

#include "rendering/RenderEnvironment.hpp"

namespace hyperion::v2 {

using renderer::RTUpdateStateFlags;

World::World()
    : EngineComponentBase(),
      m_octree(BoundingBox(Vector3(-250.0f), Vector3(250.0f)))
{
}

World::~World()
{
    Teardown();
}
    
void World::Init()
{
    if (IsInitCalled()) {
        return;
    }

    if (m_has_scene_updates.load()) {
        PerformSceneUpdates();
    }

    for (auto &it : m_scenes) {
        InitObject(it.second);
    }
    
    EngineComponentBase::Init();

    m_physics_world.Init();

    SetReady(true);

    OnTeardown([this]() {
        for (auto &it : m_scenes) {
            if (!it.second) {
                continue;
            }

            it.second->SetWorld(nullptr);
        }
        
        m_scenes.Clear();
        
        m_scene_update_mutex.lock();
        m_scenes_pending_addition.Clear();
        m_scenes_pending_removal.Clear();
        m_scene_update_mutex.unlock();

        m_physics_world.Teardown();
        
        SetReady(false);
    });
}

void World::PerformSceneUpdates()
{
    std::lock_guard guard(m_scene_update_mutex);

    for (const ID<Scene> &scene_id : m_scenes_pending_removal) {
        if (!scene_id) {
            continue;
        }

        const auto it = m_scenes.Find(scene_id);

        if (it == m_scenes.End()) {
            continue;
        }

        it->second->SetWorld(nullptr);

        m_scenes.Erase(it);
    }

    m_scenes_pending_removal.Clear();

    for (auto &scene : m_scenes_pending_addition) {
        if (!scene) {
            continue;
        }

        scene->SetWorld(this);

        m_scenes.Insert(scene->GetID(), std::move(scene));
    }

    m_scenes_pending_addition.Clear();

    m_has_scene_updates.store(false);
}

void World::Update(
    
    GameCounter::TickUnit delta
)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    m_octree.NextVisibilityState();

    for (auto &it : m_scenes) {
        m_octree.CalculateVisibility(it.second.Get());
    }

    m_physics_world.Tick(static_cast<GameCounter::TickUnitHighPrec>(delta));

    if (m_has_scene_updates.load()) {
        PerformSceneUpdates();
    }

    for (auto &it : m_scenes) {
        it.second->Update(delta);
    }
}

void World::Render(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    // set visibility cursor to previous Octree visibility cursor (atomic, relaxed)
    Engine::Get()->render_state.visibility_cursor = m_octree.LoadPreviousVisibilityCursor();
}

void World::AddScene(Handle<Scene> &&scene)
{
    if (!scene) {
        return;
    }

    InitObject(scene);

    std::lock_guard guard(m_scene_update_mutex);

    m_scenes_pending_addition.Insert(std::move(scene));

    m_has_scene_updates.store(true);
}

void World::RemoveScene(ID<Scene> id)
{
    if (!id) {
        return;
    }

    std::lock_guard guard(m_scene_update_mutex);

    m_scenes_pending_removal.Insert(id);

    m_has_scene_updates.store(true);
}

} // namespace hyperion::v2
