/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderWorld.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Renderer.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/Handle.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <scene/EnvProbe.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region RenderWorld

RenderWorld::RenderWorld(World* world)
    : m_world(world),
      m_render_environment(MakeUnique<RenderEnvironment>()),
      m_buffer_data {}
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
        });
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
        });
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
        });
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
        });
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

// void RenderWorld::RenderRemoveShadowMap(const RenderShadowMap *shadow_map)
// {
//     HYP_SCOPE;

//     if (!shadow_map) {
//         return;
//     }

//     Execute([this, shadow_map]()
//     {
//         auto it = m_shadow_map_resource_handles.FindIf([shadow_map](const TResourceHandle<RenderShadowMap> &item)
//         {
//             return item.Get() == shadow_map;
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

    HYP_LOG(Rendering, Info, "Initializing RenderWorld for World with ID: {}", m_world->GetID());

    m_render_environment->Initialize();

    UpdateBufferData();
}

void RenderWorld::Destroy_Internal()
{
    HYP_SCOPE;

    m_render_views.Clear();
}

void RenderWorld::Update_Internal()
{
    HYP_SCOPE;
}

void RenderWorld::SetBufferData(const WorldShaderData& buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
        {
            // Preserve the frame counter
            const uint32 frame_counter = m_buffer_data.frame_counter;

            m_buffer_data = buffer_data;
            m_buffer_data.frame_counter = frame_counter;

            if (IsInitialized())
            {
                UpdateBufferData();
            }
        });
}

void RenderWorld::UpdateBufferData()
{
    HYP_SCOPE;

    *static_cast<WorldShaderData*>(m_buffer_address) = m_buffer_data;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

GPUBufferHolderBase* RenderWorld::GetGPUBufferHolder() const
{
    return g_render_global_state->Worlds;
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

    RenderSetup render_setup { this, nullptr };

    m_render_environment->RenderSubsystems(frame, render_setup);

    // Collect view-independent renderable types from all views
    FixedArray<Array<RenderLight*>, uint32(LightType::MAX)> lights;
    FixedArray<Array<RenderEnvProbe*>, uint32(EnvProbeType::REFLECTION) + 1> env_probes;
    Array<RenderEnvGrid*> env_grids;

    for (const TResourceHandle<RenderView>& current_view : m_render_views)
    {
        for (uint32 light_type = 0; light_type < uint32(LightType::MAX); light_type++)
        {
            for (RenderLight* light : current_view->GetLights(LightType(light_type)))
            {
                if (lights[light_type].Contains(light))
                {
                    continue;
                }

                lights[light_type].PushBack(light);
            }
        }

        for (RenderEnvGrid* env_grid : current_view->GetEnvGrids())
        {
            if (env_grids.Contains(env_grid))
            {
                continue;
            }

            env_grids.PushBack(env_grid);
        }

        for (uint32 env_probe_type = 0; env_probe_type <= uint32(EnvProbeType::REFLECTION); env_probe_type++)
        {
            for (RenderEnvProbe* env_probe : current_view->GetEnvProbes(EnvProbeType(env_probe_type)))
            {
                if (env_probes[env_probe_type].Contains(env_probe))
                {
                    continue;
                }

                env_probes[env_probe_type].PushBack(env_probe);
            }
        }
    }

    // Set global directional light as fallback
    if (lights[uint32(LightType::DIRECTIONAL)].Any())
    {
        render_setup.light = lights[uint32(LightType::DIRECTIONAL)][0];
    }
    else
    {
        HYP_LOG(Rendering, Warning, "No directional light found in the world! EnvGrid and EnvProbe will have no fallback light set.");
        render_setup.light = nullptr;
    }

    // Set sky as fallback probe
    if (env_probes[uint32(EnvProbeType::SKY)].Any())
    {
        render_setup.env_probe = env_probes[uint32(EnvProbeType::SKY)][0];
    }
    else
    {
        HYP_LOG(Rendering, Warning, "No sky probe found in the world! EnvGrid and EnvProbe will have no fallback probe set.");
    }

    // if (env_probes.Any())
    // {
    //     for (uint32 env_probe_type = 0; env_probe_type <= uint32(EnvProbeType::REFLECTION); env_probe_type++)
    //     {
    //         for (RenderEnvProbe* env_probe : env_probes[env_probe_type])
    //         {
    //             env_probe->Render(frame, render_setup);
    //         }
    //     }
    // }

    if (env_grids.Any())
    {
        for (RenderEnvGrid* env_grid : env_grids)
        {
            env_grid->Render(frame, render_setup);
        }
    }

    render_setup.env_probe = nullptr;
    render_setup.light = nullptr;

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

    ++m_buffer_data.frame_counter;

    if (m_buffer_index != ~0u)
    {
        static_cast<WorldShaderData*>(m_buffer_address)->frame_counter = m_buffer_data.frame_counter;
        GetGPUBufferHolder()->MarkDirty(m_buffer_index);
    }
}

#pragma endregion RenderWorld

namespace renderer {

HYP_DESCRIPTOR_CBUFF(Global, WorldsBuffer, 1, sizeof(WorldShaderData), true);

} // namespace renderer

} // namespace hyperion