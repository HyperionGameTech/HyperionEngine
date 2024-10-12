/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Engine.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/DirectionalLightShadowRenderer.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <core/system/AppContext.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(RemoveAllRenderComponents) : renderer::RenderCommand
{
    TypeMap<FlatMap<Name, RC<RenderComponentBase>>> render_components;

    RENDER_COMMAND(RemoveAllRenderComponents)(TypeMap<FlatMap<Name, RC<RenderComponentBase>>> &&render_components)
        : render_components(std::move(render_components))
    {
    }

    virtual Result operator()()
    {
        Result result;

        for (auto &it : render_components) {
            for (auto &component_tag_pair : it.second) {
                if (component_tag_pair.second == nullptr) {
                    DebugLog(
                        LogType::Warn,
                        "RenderComponent with name %s was null, skipping...\n",
                        component_tag_pair.first.LookupString()
                    );

                    continue;
                }

                component_tag_pair.second->ComponentRemoved();
            }
        }

        return result;
    }
};

#pragma endregion Render commands

RenderEnvironment::RenderEnvironment(Scene *scene)
    : BasicObject(),
      m_scene(scene),
      m_frame_counter(0),
      m_current_enabled_render_components_mask(0),
      m_next_enabled_render_components_mask(0),
      m_ddgi({
          .aabb = {{-25.0f, -5.0f, -25.0f}, {25.0f, 30.0f, 25.0f}}
      }),
      m_has_rt_radiance(false),
      m_has_ddgi_probes(false)
{
}

RenderEnvironment::~RenderEnvironment()
{
    m_particle_system.Reset();

    m_gaussian_splatting.Reset();
        
    if (m_has_rt_radiance) {
        m_rt_radiance->Destroy();
        m_rt_radiance.Reset();

        m_has_rt_radiance = false;
    }

    if (m_has_ddgi_probes) {
        m_ddgi.Destroy();

        m_has_ddgi_probes = false;
    }

    const auto update_marker_value = m_update_marker.Get(MemoryOrder::ACQUIRE);

    PUSH_RENDER_COMMAND(RemoveAllRenderComponents, std::move(m_render_components));

    // @TODO Call ComponentRemoved() for all components pending addition as well

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS) {
        std::lock_guard guard(m_render_component_mutex);

        m_render_components_pending_addition.Clear();
        m_render_components_pending_removal.Clear();
    }

    m_update_marker.BitAnd(~RENDER_ENVIRONMENT_UPDATES_CONTAINERS, MemoryOrder::RELEASE);

    SafeRelease(std::move(m_tlas));

    HYP_SYNC_RENDER();
}

void RenderEnvironment::SetTLAS(const TLASRef &tlas)
{
    m_tlas = tlas;

    m_update_marker.BitOr(RENDER_ENVIRONMENT_UPDATES_TLAS, MemoryOrder::RELEASE);
}

void RenderEnvironment::Init()
{
    if (IsInitCalled()) {
        return;
    }
    
    BasicObject::Init();

    m_particle_system = CreateObject<ParticleSystem>();
    InitObject(m_particle_system);

    m_gaussian_splatting = CreateObject<GaussianSplatting>();
    InitObject(m_gaussian_splatting);
    
    m_rt_radiance = MakeUnique<RTRadianceRenderer>(
        Extent2D { 1024, 1024 },
        g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.path_tracer.enabled").ToBool()
            ? RT_RADIANCE_RENDERER_OPTION_PATHTRACER
            : RT_RADIANCE_RENDERER_OPTION_NONE
    );
    
    if (m_tlas) {
        m_rt_radiance->SetTLAS(m_tlas);
        m_rt_radiance->Create();

        m_ddgi.SetTLAS(m_tlas);
        m_ddgi.Init();

        m_has_rt_radiance = true;
        m_has_ddgi_probes = true;
    }

    {
        std::lock_guard guard(m_render_component_mutex);
        
        for (auto &it : m_render_components_pending_addition) {
            for (auto &component_tag_pair : it.second) {
                AssertThrow(component_tag_pair.second != nullptr);

                component_tag_pair.second->ComponentInit();
            }
        }
    }

    SetReady(true);
}

void RenderEnvironment::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertReady();

    std::lock_guard guard(m_render_component_mutex);

    for (const auto &it : m_render_components) {
        for (const auto &component_tag_pair : it.second) {
            component_tag_pair.second->ComponentUpdate(delta);
        }
    }
}

void RenderEnvironment::ApplyTLASUpdates(Frame *frame, RTUpdateStateFlags flags)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    AssertReady();

    AssertThrow(g_engine->GetGPUDevice()->GetFeatures().IsRaytracingSupported());
    
    if (m_has_rt_radiance) {
        m_rt_radiance->ApplyTLASUpdates(flags);
    }

    if (m_has_ddgi_probes) {
        m_ddgi.ApplyTLASUpdates(flags);
    }
}

void RenderEnvironment::RenderRTRadiance(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    AssertReady();
    
    if (m_has_rt_radiance) {
        m_rt_radiance->Render(frame);
    }
}

void RenderEnvironment::RenderDDGIProbes(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    AssertReady();

    AssertThrow(g_engine->GetGPUDevice()->GetFeatures().IsRaytracingSupported());
    
    if (m_has_ddgi_probes) {
        const DirectionalLightShadowRenderer *shadow_map_renderer = GetRenderComponent<DirectionalLightShadowRenderer>();

        m_ddgi.RenderProbes(frame);
        m_ddgi.ComputeIrradiance(frame);

        if (g_engine->GetConfig().Get(CONFIG_RT_GI_DEBUG_PROBES).GetBool()) {
            for (const Probe &probe : m_ddgi.GetProbes()) {
                g_engine->GetDebugDrawer()->Sphere(probe.position);
            }
        }
    }
}

void RenderEnvironment::RenderComponents(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    AssertReady();

    m_current_enabled_render_components_mask = m_next_enabled_render_components_mask;

    const RenderEnvironmentUpdates update_marker_value = m_update_marker.Get(MemoryOrder::ACQUIRE);
    RenderEnvironmentUpdates inverse_mask = 0;

    for (const auto &it : m_render_components) {
        for (const auto &component_tag_pair : it.second) {
            // m_next_enabled_render_components_mask |= 1u << uint32(component_tag_pair.second->GetName());

            component_tag_pair.second->ComponentRender(frame);
        }
    }

    // add RenderComponents AFTER rendering, so that the render scheduler can complete
    // initialization tasks before we call ComponentRender() on the newly added RenderComponents

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS) {
        inverse_mask |= RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS;

        std::lock_guard guard(m_render_component_mutex);

        for (auto &it : m_render_components_pending_addition) {
            auto &items_map = it.second;
            const auto components_it = m_render_components.Find(it.first);

            if (components_it != m_render_components.End()) {
                for (auto &items_pending_addition_it : items_map) {
                    const Name name = items_pending_addition_it.first;

                    const auto name_it = components_it->second.Find(name);

                    AssertThrowMsg(
                        name_it == components_it->second.End(),
                        "Render component with name %s already exists! Name must be unique.\n",
                        name.LookupString()
                    );

                    components_it->second.Set(name, std::move(items_pending_addition_it.second));
                }
            } else {
                m_render_components.Set(it.first, std::move(it.second));
            }
        }

        m_render_components_pending_addition.Clear();

        for (const auto &it : m_render_components_pending_removal) {
            const Name name = it.second;

            const auto components_it = m_render_components.Find(it.first);

            if (components_it != m_render_components.End()) {
                auto &items_map = components_it->second;

                const auto item_it = items_map.Find(name);

                if (item_it != items_map.End()) {
                    const RC<RenderComponentBase> &item = item_it->second;

                    if (item != nullptr) {
                        item->ComponentRemoved();
                    }

                    items_map.Erase(item_it);
                }

                if (items_map.Empty()) {
                    m_render_components.Erase(components_it);
                }
            }
        }

        m_render_components_pending_removal.Clear();
    }

    if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool()) {
        if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_TLAS) {
            inverse_mask |= RENDER_ENVIRONMENT_UPDATES_TLAS;

            if (m_tlas) {
                if (m_has_rt_radiance) {
                    m_rt_radiance->Destroy();
                }

                if (m_has_ddgi_probes) {
                    m_ddgi.Destroy();
                }
                
                m_ddgi.SetTLAS(m_tlas);
                m_rt_radiance->SetTLAS(m_tlas);

                m_rt_radiance->Create();
                m_ddgi.Init();

                m_has_rt_radiance = true;
                m_has_ddgi_probes = true;

                HYP_SYNC_RENDER();
            } else {
                if (m_has_rt_radiance) {
                    m_rt_radiance->Destroy();
                }

                if (m_has_ddgi_probes) {
                    m_ddgi.Destroy();
                }

                m_has_rt_radiance = false;
                m_has_ddgi_probes = false;

                HYP_SYNC_RENDER();
            }
        }

        // for RT, we may need to perform resizing of buffers, and thus modification of descriptor sets
        AssertThrow(m_scene != nullptr);

        if (const TLASRef &tlas = m_scene->GetTLAS()) {
            RTUpdateStateFlags update_state_flags = renderer::RT_UPDATE_STATE_FLAGS_NONE;

            tlas->UpdateStructure(
                g_engine->GetGPUInstance(),
                update_state_flags
            );

            if (update_state_flags) {
                ApplyTLASUpdates(frame, update_state_flags);
            }
        }
    }

    if (update_marker_value) {
        m_update_marker.BitAnd(~inverse_mask, MemoryOrder::RELEASE);
    }

    ++m_frame_counter;
}

} // namespace hyperion
