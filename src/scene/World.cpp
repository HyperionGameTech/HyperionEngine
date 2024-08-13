/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/World.hpp>

#ifdef HYP_EDITOR
#include <editor/EditorSubsystem.hpp>
#endif

#include <scene/world_grid/WorldGridSubsystem.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/utilities/Format.hpp>

#include <core/object/HypClassUtils.hpp>

#include <rendering/RenderEnvironment.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DEFINE_CLASS(
    World,
    HypProperty(NAME("ID"), &World::GetID),
    HypProperty(NAME("GameTime"), &World::GetGameTime)
);

using renderer::RTUpdateStateFlags;

#define HYP_WORLD_ASYNC_SCENE_UPDATES

World::World()
    : BasicObject(),
      m_detached_scenes(this),
      m_has_scene_updates(false),
      m_game_time(0.0)
{
#ifdef HYP_EDITOR
    AddSubsystem<EditorSubsystem>();
#endif

    AddSubsystem<WorldGridSubsystem>();
}

World::~World()
{
    if (IsInitCalled()) {
        for (Handle<Scene> &scene : m_scenes) {
            if (!scene.IsValid()) {
                continue;
            }

            for (auto &it : m_subsystems) {
                it.second->OnSceneDetached(scene);
            }

            scene->SetWorld(nullptr);
        }

        for (auto &it : m_subsystems) {
            it.second->Shutdown();
        }
    }

    m_physics_world.Teardown();
}
    
void World::Init()
{
    if (IsInitCalled()) {
        return;
    }

    for (auto &it : m_subsystems) {
        it.second->Initialize();
    }

    for (Handle<Scene> &scene : m_scenes) {
        InitObject(scene);

        for (auto &it : m_subsystems) {
            it.second->OnSceneAttached(scene);
        }
    }

    UpdatePendingScenes();
    
    BasicObject::Init();

    m_physics_world.Init();

    SetReady(true);
}

void World::UpdatePendingScenes()
{
    HYP_SCOPE;

    Mutex::Guard guard(m_scene_update_mutex);

    for (ID<Scene> id : m_scenes_pending_removal) {
        if (!id.IsValid()) {
            continue;
        }

        const auto it = m_scenes.FindIf([id](const Handle<Scene> &scene)
        {
            return scene->GetID() == id;
        });

        if (it == m_scenes.End()) {
            continue;
        }
        
        m_render_list_container.RemoveScene(it->Get());
    }

    m_scenes_pending_removal.Clear();

    for (ID<Scene> id : m_scenes_pending_addition) {
        if (!id.IsValid()) {
            continue;
        }

        const auto it = m_scenes.FindIf([id](const Handle<Scene> &scene)
        {
            return scene->GetID() == id;
        });

        if (it == m_scenes.End()) {
            continue;
        }

        m_render_list_container.AddScene(it->Get());
    }

    m_scenes_pending_addition.Clear();

    m_has_scene_updates.Set(false, MemoryOrder::RELEASE);
}

void World::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertReady();

    m_physics_world.Tick(delta);

    if (m_has_scene_updates.Get(MemoryOrder::ACQUIRE)) {
        UpdatePendingScenes();
    }

    Array<Task<void>> update_subsystem_tasks;
    update_subsystem_tasks.Reserve(m_subsystems.Size());

    for (auto &it : m_subsystems) {
        if (it.second->RequiresUpdateOnGameThread()) {
            continue;
        }

        update_subsystem_tasks.PushBack(TaskSystem::GetInstance().Enqueue([this, subsystem = it.second.Get(), delta]
        {
            HYP_NAMED_SCOPE_FMT("Update subsystem: {}", subsystem->GetName());

            subsystem->Update(delta);
        }));
    }

    for (auto &it : m_subsystems) {
        if (!it.second->RequiresUpdateOnGameThread()) {
            continue;
        }

        it.second->Update(delta);
    }

    for (Task<void> &task : update_subsystem_tasks) {
        task.Await();
    }

    for (uint32 index = 0; index < m_scenes.Size(); index++) {
        if (!m_scenes[index].IsValid()) {
            continue;
        }

        if (!m_scenes[index]->IsWorldScene()) {
            continue;
        }

        if (RenderEnvironment *render_environment = m_scenes[index]->GetEnvironment()) {
            HYP_NAMED_SCOPE_FMT("Update RenderEnvironment for Scene with ID #{}", m_scenes[index]->GetID().Value());

            render_environment->Update(delta);
        }
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

    m_game_time += delta;
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

    const uint32 num_render_lists = m_render_list_container.NumRenderLists();

    for (uint32 i = 0; i < num_render_lists; i++) {
        if (RenderEnvironment *render_environment = m_render_list_container.GetRenderListAtIndex(i)->GetRenderEnvironment()) {
            render_environment->RenderComponents(frame);
        }
    }
}

SubsystemBase *World::AddSubsystem(TypeID type_id, UniquePtr<SubsystemBase> &&subsystem)
{
    HYP_SCOPE;

    // If World is already initialized, ensure that the call is made on the correct thread
    // otherwise, it is safe to call this function from the World constructor (main thread)
    if (IsInitCalled()) {
        Threads::AssertOnThread(ThreadName::THREAD_GAME);
    } else {
        Threads::AssertOnThread(ThreadName::THREAD_MAIN);
    }

    const auto it = m_subsystems.Find(type_id);
    AssertThrowMsg(it == m_subsystems.End(), "Subsystem already exists in World");

    auto insert_result = m_subsystems.Set(type_id, std::move(subsystem));

    // If World is already initialized, initialize the subsystem
    // otherwise, it will be initialized when World::Init() is called
    if (IsInitCalled()) {
        if (insert_result.second) {
            insert_result.first->second->Initialize();

            for (Handle<Scene> &scene : m_scenes) {
                insert_result.first->second->OnSceneAttached(scene);
            }
        }
    }

    return insert_result.first->second.Get();
}

SubsystemBase *World::GetSubsystem(TypeID type_id)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME | ThreadName::THREAD_TASK);

    const auto it = m_subsystems.Find(type_id);

    if (it == m_subsystems.End()) {
        return nullptr;
    }

    return it->second.Get();
}

void World::AddScene(const Handle<Scene> &scene)
{
    AddScene(Handle<Scene>(scene));
}

void World::AddScene(Handle<Scene> &&scene)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    if (!scene.IsValid()) {
        return;
    }

    if (IsInitCalled()) {
        scene->SetWorld(this);
        InitObject(scene);

        for (auto &it : m_subsystems) {
            it.second->OnSceneAttached(scene);
        }
    }

    const ID<Scene> scene_id = scene->GetID();

    m_scenes.PushBack(std::move(scene));

    Mutex::Guard guard(m_scene_update_mutex);

    m_scenes_pending_addition.Insert(scene_id);
    m_has_scene_updates.Set(true, MemoryOrder::RELEASE);
}

void World::RemoveScene(const WeakHandle<Scene> &scene_weak)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    typename Array<Handle<Scene>>::Iterator it = m_scenes.FindAs(scene_weak);
    
    if (it == m_scenes.End()) {
        return;
    }

    Handle<Scene> scene = *it;

    if (!scene.IsValid()) {
        return;
    }

    if (IsInitCalled()) {
        for (auto &it : m_subsystems) {
            it.second->OnSceneDetached(scene);
        }

        scene->SetWorld(nullptr);
    }

    Mutex::Guard guard(m_scene_update_mutex);

    m_scenes_pending_removal.Insert(scene->GetID());
    m_has_scene_updates.Set(true, MemoryOrder::RELEASE);
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
