/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/World.hpp>
#include <rendering/Camera.hpp>
#include <rendering/Scene.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/Handle.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region RenderCollectorContainer

void RenderCollectorContainer::AddScene(const Scene *scene)
{
    AssertThrow(scene != nullptr);
    AssertThrowMsg(scene->GetCamera().IsValid(), "Cannot acquire RenderCollector for Scene with no Camera attached.");

    const uint32 scene_index = scene->GetID().ToIndex();
    AssertThrow(scene_index < max_scenes);

    RenderCollector &render_collector = m_render_collectors_by_id_index[scene_index];
    render_collector.SetCamera(scene->GetCamera());

    if (scene->IsForegroundScene()) {
        render_collector.SetRenderEnvironment(scene->GetRenderResource().GetEnvironment());
    } else {
        render_collector.SetRenderEnvironment(WeakHandle<RenderEnvironment> { });
    }

    const uint32 render_collector_index = m_num_render_collectors.Increment(1u, MemoryOrder::ACQUIRE_RELEASE);
    m_render_collectors[render_collector_index] = &render_collector;
}

void RenderCollectorContainer::RemoveScene(ID<Scene> id)
{
    AssertThrow(id.IsValid());
    
    m_num_render_collectors.Decrement(1u, MemoryOrder::RELEASE);

    RenderCollector &render_collector = m_render_collectors_by_id_index[id.ToIndex()];
    render_collector.SetCamera(Handle<Camera>::empty);
    render_collector.SetRenderEnvironment(WeakHandle<RenderEnvironment> { });
    render_collector.Reset();
}

#pragma endregion RenderCollectorContainer

#pragma region WorldRenderResource

WorldRenderResource::WorldRenderResource(World *world)
    : m_world(world)
{
}

WorldRenderResource::~WorldRenderResource() = default;

void WorldRenderResource::AddScene(const Handle<Scene> &scene)
{
    HYP_SCOPE;

    if (!scene.IsValid()) {
        return;
    }

    Execute([this, scene_weak = scene.ToWeak()]()
    {
        if (Handle<Scene> scene = scene_weak.Lock()) {
            m_render_collector_container.AddScene(scene.Get());

            m_bound_scenes.PushBack(TResourceHandle(scene->GetRenderResource()));
        }
    });          
}

Task<bool> WorldRenderResource::RemoveScene(ID<Scene> scene_id)
{
    HYP_SCOPE;

    Task<bool> task;
    auto task_executor = task.Initialize();

    Execute([this, scene_id, task_executor]()
    {
        m_render_collector_container.RemoveScene(scene_id);

        auto bound_scenes_it = m_bound_scenes.FindIf([&scene_id](const TResourceHandle<SceneRenderResource> &item)
        {
            return item->GetScene()->GetID() == scene_id;
        });

        if (bound_scenes_it != m_bound_scenes.End()) {
            m_bound_scenes.Erase(bound_scenes_it);

            task_executor->Fulfill(true);
        } else {
            task_executor->Fulfill(false);
        }
    });

    return task;
}

void WorldRenderResource::Initialize_Internal()
{
    HYP_SCOPE;
}

void WorldRenderResource::Destroy_Internal()
{
    HYP_SCOPE;

    m_bound_cameras.Clear();
    m_bound_scenes.Clear();
}

void WorldRenderResource::Update_Internal()
{
    HYP_SCOPE;
}

GPUBufferHolderBase *WorldRenderResource::GetGPUBufferHolder() const
{
    return nullptr;
}

void WorldRenderResource::PreRender(renderer::Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    const uint32 num_render_collectors = m_render_collector_container.NumRenderCollectors();

    for (uint32 i = 0; i < num_render_collectors; i++) {
        RenderCollector *render_collector = m_render_collector_container.GetRenderCollectorAtIndex(i);

        if (const Handle<Camera> &camera = render_collector->GetCamera()) {
            m_bound_cameras.PushBack(TResourceHandle<CameraRenderResource>(camera->GetRenderResource()));
        }
    }
}

void WorldRenderResource::PostRender(renderer::Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    m_bound_cameras.Clear();
}

void WorldRenderResource::Render(renderer::Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    const uint32 num_render_collectors = m_render_collector_container.NumRenderCollectors();

    for (uint32 i = 0; i < num_render_collectors; i++) {
        RenderCollector *render_collector = m_render_collector_container.GetRenderCollectorAtIndex(i);

        if (const WeakHandle<RenderEnvironment> &render_environment = render_collector->GetRenderEnvironment()) {
            render_environment.GetUnsafe()->RenderSubsystems(frame);
        }
    }
}

#pragma endregion WorldRenderResource

} // namespace hyperion