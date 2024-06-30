/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/World.hpp>

#include <core/threading/Threads.hpp>

#include <core/HypClassUtils.hpp>

#include <rendering/RenderEnvironment.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DEFINE_CLASS(World);

using renderer::RTUpdateStateFlags;

#define HYP_WORLD_ASYNC_SCENE_UPDATES

World::World()
    : BasicObject(),
      m_detached_scenes(this)
{
}

World::~World()
{
    for (Handle<Scene> &scene : m_scenes) {
        if (!scene) {
            continue;
        }

        scene->SetWorld(nullptr);
    }
    
    m_scenes.Clear();
    m_scenes_pending_addition.Clear();
    m_scenes_pending_removal.Clear();

    m_physics_world.Teardown();
}
    
void World::Init()
{
    if (IsInitCalled()) {
        return;
    }

    if (m_has_scene_updates.Get(MemoryOrder::RELAXED)) {
        PerformSceneUpdates();
    }

    for (Handle<Scene> &scene : m_scenes) {
        InitObject(scene);
    }
    
    BasicObject::Init();

    m_physics_world.Init();

    SetReady(true);
}

void World::PerformSceneUpdates()
{
    HYP_SCOPE;

    Mutex::Guard guard(m_scene_update_mutex);

    for (Handle<Scene> &scene : m_scenes_pending_removal) {
        if (!scene) {
            continue;
        }

        const auto it = m_scenes.Find(scene);

        if (it == m_scenes.End()) {
            continue;
        }

        scene->SetWorld(nullptr);
        
        m_render_list_container.RemoveScene(scene.Get());

        m_scenes.Erase(it);
    }

    m_scenes_pending_removal.Clear();

    for (Handle<Scene> &scene : m_scenes_pending_addition) {
        if (!scene) {
            continue;
        }

        scene->SetWorld(this);

        m_render_list_container.AddScene(scene.Get());

        m_scenes.Insert(std::move(scene));
    }

    m_scenes_pending_addition.Clear();

    m_has_scene_updates.Set(false, MemoryOrder::RELAXED);
}

void World::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertReady();

    m_physics_world.Tick(delta);

    if (m_has_scene_updates.Get(MemoryOrder::RELAXED)) {
        PerformSceneUpdates();
    }

#ifdef HYP_WORLD_ASYNC_SCENE_UPDATES
    Array<Task<void>> collect_entities_tasks;
    collect_entities_tasks.Reserve(m_scenes.Size());
#endif

    for (uint32 index = 0; index < m_scenes.Size(); index++) {
        HYP_NAMED_SCOPE("Begin scene updates");

        m_scenes[index]->BeginUpdate(delta);
    }

    for (uint32 index = 0; index < m_scenes.Size(); index++) {
        HYP_NAMED_SCOPE("End scene updates");

        const Handle<Scene> &scene = m_scenes[index];

        scene->EndUpdate();

#ifdef HYP_WORLD_ASYNC_SCENE_UPDATES
        collect_entities_tasks.PushBack(TaskSystem::GetInstance().Enqueue([this, index]
        {
            const Handle<Scene> &scene = m_scenes[index];

            RenderList &render_list = m_render_list_container.GetRenderListForScene(scene->GetID());

            scene->CollectEntities(render_list, scene->GetCamera());
        }));
#endif
    }

#ifdef HYP_WORLD_ASYNC_SCENE_UPDATES
    for (Task<void> &task : collect_entities_tasks) {
        task.Await();
    }
#endif
}

void World::PreRender(Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    AssertReady();
}

void World::Render(Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    AssertReady();

    // TODO: Thread safe list for scenes
    for (auto &scene : m_scenes) {
        if (!scene->GetEnvironment() || !scene->GetEnvironment()->IsReady()) {
            continue;
        }
 
        scene->GetEnvironment()->RenderComponents(frame);
    }
}

void World::AddScene(const Handle<Scene> &scene)
{
    AddScene(Handle<Scene>(scene));
}

void World::AddScene(Handle<Scene> &&scene)
{
    if (!scene) {
        return;
    }

    HYP_SCOPE;

    InitObject(scene);

    Mutex::Guard guard(m_scene_update_mutex);

    m_scenes_pending_addition.Insert(std::move(scene));

    m_has_scene_updates.Set(true, MemoryOrder::RELAXED);
}

void World::RemoveScene(ID<Scene> id)
{
    if (!id) {
        return;
    }

    HYP_SCOPE;

    Mutex::Guard guard(m_scene_update_mutex);

    m_scenes_pending_removal.Insert(Handle<Scene>(id));

    m_has_scene_updates.Set(true, MemoryOrder::RELAXED);
}

const Handle<Scene> &World::GetDetachedScene(ThreadName thread_name)
{
    const ThreadID thread_id = Threads::GetThreadID(thread_name);

    Threads::AssertOnThread(thread_id);

    return GetDetachedScene(thread_id);
}

const Handle<Scene> &World::GetDetachedScene(ThreadID thread_id)
{
    return m_detached_scenes.GetDetachedScene(thread_id);
}

} // namespace hyperion
