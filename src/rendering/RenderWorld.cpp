/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderWorld.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/Shadows.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/Handle.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region RenderWorld

RenderWorld::RenderWorld(World* world)
    : m_world(world),
      m_shadow_map_manager(MakeUnique<ShadowMapManager>())
{
}

RenderWorld::~RenderWorld() = default;

void RenderWorld::AddView(TResourceHandle<RenderView>&& render_view)
{
    HYP_SCOPE;

    if (!render_view)
    {
        return;
    }

    Execute([this, render_view = std::move(render_view)]()
        {
            m_render_views.PushBack(std::move(render_view));

            std::sort(m_render_views.Begin(), m_render_views.End(), [](const TResourceHandle<RenderView>& a, const TResourceHandle<RenderView>& b)
                {
                    return uint32(b->GetPriority()) < uint32(a->GetPriority());
                });
        },
        /* force_owner_thread */ true);
}

void RenderWorld::RemoveView(RenderView* render_view)
{
    HYP_SCOPE;

    if (!render_view)
    {
        return;
    }

    Execute([this, render_view]()
        {
            auto it = m_render_views.FindIf([render_view](const TResourceHandle<RenderView>& item)
                {
                    return item.Get() == render_view;
                });

            if (it != m_render_views.End())
            {
                RenderView* render_view = it->Get();
                it->Reset();

                m_render_views.Erase(it);
            }
        },
        /* force_owner_thread */ true);
}

void RenderWorld::RemoveViewsForScene(const WeakHandle<Scene>& scene_weak)
{
    HYP_SCOPE;

    if (!scene_weak.IsValid())
    {
        return;
    }

    Execute([this, scene_weak]()
        {
            for (auto it = m_render_views.Begin(); it != m_render_views.End();)
            {
                if (!(*it)->GetScene())
                {
                    ++it;

                    continue;
                }

                if ((*it)->GetScene()->GetScene() == scene_weak.GetUnsafe())
                {
                    RenderView* render_view = it->Get();

                    it->Reset();

                    it = m_render_views.Erase(it);
                }
                else
                {
                    ++it;
                }
            }
        },
        /* force_owner_thread */ true);
}

void RenderWorld::AddScene(TResourceHandle<RenderScene>&& render_scene)
{
    HYP_SCOPE;

    if (!render_scene)
    {
        return;
    }

    Execute([this, render_scene = std::move(render_scene)]()
        {
            m_render_scenes.PushBack(std::move(render_scene));
        },
        /* force_owner_thread */ true);
}

void RenderWorld::RemoveScene(RenderScene* render_scene)
{
    HYP_SCOPE;

    if (!render_scene)
    {
        return;
    }

    Execute([this, render_scene]()
        {
            auto it = m_render_scenes.FindIf([render_scene](const TResourceHandle<RenderScene>& item)
                {
                    return item.Get() == render_scene;
                });

            if (it != m_render_scenes.End())
            {
                m_render_scenes.Erase(it);
            }
        },
        /* force_owner_thread */ true);
}

// void RenderWorld::RenderAddShadowMap(const TResourceHandle<RenderShadowMap> &shadow_map_resource_handle)
// {
//     HYP_SCOPE;

//     if (!shadow_map_resource_handle) {
//         return;
//     }

//     Execute([this, shadow_map_resource_handle]()
//     {
//         m_shadow_map_resource_handles.PushBack(std::move(shadow_map_resource_handle));
//     }, /* force_owner_thread */ true);
// }

// void RenderWorld::RenderRemoveShadowMap(const RenderShadowMap *shadow_render_map)
// {
//     HYP_SCOPE;

//     if (!shadow_render_map) {
//         return;
//     }

//     Execute([this, shadow_render_map]()
//     {
//         auto it = m_shadow_map_resource_handles.FindIf([shadow_render_map](const TResourceHandle<RenderShadowMap> &item)
//         {
//             return item.Get() == shadow_render_map;
//         });

//         if (it != m_shadow_map_resource_handles.End()) {
//             m_shadow_map_resource_handles.Erase(it);
//         }
//     }, /* force_owner_thread */ true);
// }

const EngineRenderStats& RenderWorld::GetRenderStats() const
{
    HYP_SCOPE;
    if (Threads::IsOnThread(g_render_thread))
    {
        return m_render_stats[ThreadType::THREAD_TYPE_RENDER];
    }
    else if (Threads::IsOnThread(g_game_thread))
    {
        return m_render_stats[ThreadType::THREAD_TYPE_GAME];
    }
    else
    {
        HYP_UNREACHABLE();
    }
}

void RenderWorld::SetRenderStats(const EngineRenderStats& render_stats)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    m_render_stats[ThreadType::THREAD_TYPE_GAME] = render_stats;

    Execute([this, render_stats]()
        {
            m_render_stats[ThreadType::THREAD_TYPE_RENDER] = render_stats;
        });
}

void RenderWorld::Initialize_Internal()
{
    HYP_SCOPE;

    m_shadow_map_manager->Initialize();
}

void RenderWorld::Destroy_Internal()
{
    HYP_SCOPE;

    m_render_views.Clear();

    m_shadow_map_manager->Destroy();
}

void RenderWorld::Update_Internal()
{
    HYP_SCOPE;
}

GPUBufferHolderBase* RenderWorld::GetGPUBufferHolder() const
{
    return nullptr;
}

void RenderWorld::PreRender(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    for (const TResourceHandle<RenderView>& current_view : m_render_views)
    {
        current_view->PreRender(frame);
    }
}

void RenderWorld::Render(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    for (const TResourceHandle<RenderScene>& render_scene : m_render_scenes)
    {
        render_scene->GetEnvironment()->RenderSubsystems(frame);
    }

    for (const TResourceHandle<RenderView>& current_view : m_render_views)
    {
        current_view->Render(frame, this);
    }
}

void RenderWorld::PostRender(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    for (const TResourceHandle<RenderView>& current_view : m_render_views)
    {
        current_view->PostRender(frame);
    }
}

#pragma endregion RenderWorld

} // namespace hyperion