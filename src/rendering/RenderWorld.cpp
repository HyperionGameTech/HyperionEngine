/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderWorld.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/Handle.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region WorldRenderResource

WorldRenderResource::WorldRenderResource(World *world)
    : m_world(world)
{
    m_environment = CreateObject<RenderEnvironment>();
    InitObject(m_environment);
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
            m_bound_scenes.PushBack(TResourceHandle<SceneRenderResource>(scene->GetRenderResource()));
        }
    }, /* force_owner_thread */ true);          
}

Task<bool> WorldRenderResource::RemoveScene(const WeakHandle<Scene> &scene_weak)
{
    HYP_SCOPE;

    Task<bool> task;
    auto task_executor = task.Initialize();

    if (!scene_weak.IsValid()) {
        task_executor->Fulfill(false);

        return task;
    }

    Execute([this, scene_weak, task_executor]()
    {
        const ID<Scene> scene_id = scene_weak.GetID();

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
    }, /* force_owner_thread */ true);

    return task;
}

const EngineRenderStats &WorldRenderResource::GetRenderStats() const
{
    HYP_SCOPE;
    if (Threads::IsOnThread(g_render_thread)) {
        return m_render_stats[ThreadType::THREAD_TYPE_RENDER];
    } else if (Threads::IsOnThread(g_game_thread)) {
        return m_render_stats[ThreadType::THREAD_TYPE_GAME];
    } else {
        HYP_UNREACHABLE();
    }
}

void WorldRenderResource::SetRenderStats(const EngineRenderStats &render_stats)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    m_render_stats[ThreadType::THREAD_TYPE_GAME] = render_stats;

    Execute([this, render_stats]()
    {
        m_render_stats[ThreadType::THREAD_TYPE_RENDER] = render_stats;
    });
}

void WorldRenderResource::Initialize_Internal()
{
    HYP_SCOPE;
}

void WorldRenderResource::Destroy_Internal()
{
    HYP_SCOPE;

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

void WorldRenderResource::PreRender(renderer::IFrame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);
}

void WorldRenderResource::PostRender(renderer::IFrame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);
}

void WorldRenderResource::Render(renderer::IFrame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    m_environment->RenderSubsystems(frame);
}

#pragma endregion WorldRenderResource

} // namespace hyperion