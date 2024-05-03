/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/World.hpp>

#include <core/threading/Threads.hpp>

#include <rendering/RenderEnvironment.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::RTUpdateStateFlags;

World::World()
    : BasicObject()
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

    for (auto &it : m_scenes) {
        DebugLog(LogType::Debug, "~World, scene ID: %u\n", it->GetID().value);
    }

    for (auto &it : m_scenes_pending_addition) {
        DebugLog(LogType::Debug, "~World, pending add scene ID: %u\n", it->GetID().value);
    }

    for (auto &it : m_scenes_pending_removal) {
        DebugLog(LogType::Debug, "~World, pending removal scene ID: %u\n", it->GetID().value);
    }
    
    m_scenes.Clear();
    m_scenes_pending_addition.Clear();
    m_scenes_pending_removal.Clear();
    m_detached_scenes.Clear();

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
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertReady();

    m_physics_world.Tick(delta);

    if (m_has_scene_updates.Get(MemoryOrder::RELAXED)) {
        PerformSceneUpdates();
    }

    for (Handle<Scene> &scene : m_scenes) {
        scene->Update(delta);

        RenderList &render_list = m_render_list_container.GetRenderListForScene(scene->GetID());

        scene->CollectEntities(render_list, scene->GetCamera());
    }
}

void World::PreRender(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    AssertReady();
}

void World::Render(Frame *frame)
{
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

    Mutex::Guard guard(m_scene_update_mutex);

    m_scenes_pending_removal.Insert(Handle<Scene>(id));

    m_has_scene_updates.Set(true, MemoryOrder::RELAXED);
}

const Handle<Scene> &World::GetDetachedScene()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    return GetDetachedScene(ThreadName::THREAD_GAME);
}

const Handle<Scene> &World::GetDetachedScene(ThreadMask thread_mask)
{
    Mutex::Guard guard(m_detached_scenes_mutex);

    auto it = m_detached_scenes.Find(thread_mask);

    if (it == m_detached_scenes.End()) {
        auto scene = CreateObject<Scene>(
            Handle<Camera>::empty,
            Scene::InitInfo {
                .thread_mask = thread_mask
            }
        );

        scene->SetName(Name::Unique("DetachedScene"));

        InitObject(scene);

        scene->SetWorld(this);

        it = m_detached_scenes.Insert(thread_mask, std::move(scene)).first;
    }

    return it->second;
}

} // namespace hyperion
