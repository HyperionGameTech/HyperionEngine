#include "RenderEnvironment.hpp"
#include <Engine.hpp>

#include <rendering/backend/RendererFrame.hpp>

namespace hyperion::v2 {

using renderer::Result;

struct RENDER_COMMAND(RemoveAllRenderComponents) : RenderCommand
{
    TypeMap<FlatMap<Name, UniquePtr<RenderComponentBase>>> render_components;

    RENDER_COMMAND(RemoveAllRenderComponents)(TypeMap<FlatMap<Name, UniquePtr<RenderComponentBase>>> &&render_components)
        : render_components(std::move(render_components))
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        for (auto &it : render_components) {
            for (auto &component_tag_pair : it.second) {
                if (component_tag_pair.second == nullptr) {
                    DebugLog(
                        LogType::Warn,
                        "RenderComponent with name %s was null, skipping...\n",
                        component_tag_pair.first.LookupString().Data()
                    );

                    continue;
                }

                component_tag_pair.second->ComponentRemoved();
            }
        }

        return result;
    }
};

RenderEnvironment::RenderEnvironment(Scene *scene)
    : EngineComponentBase(),
      m_scene(scene),
      m_global_timer(0.0f),
      m_frame_counter(0),
      m_current_enabled_render_components_mask(0),
      m_next_enabled_render_components_mask(0),
      m_rt_radiance(Extent2D { 1024, 1024 }),
      m_probe_system({
          .aabb = {{-15.0f, -5.0f, -15.0f}, {15.0f, 25.0f, 15.0f}}
      }),
      m_has_rt_radiance(false),
      m_has_ddgi_probes(false)
{
}

RenderEnvironment::~RenderEnvironment()
{
    Teardown();
}

void RenderEnvironment::SetTLAS(const Handle<TLAS> &tlas)
{
    m_tlas = tlas;

    InitObject(m_tlas);

    m_update_marker.fetch_or(RENDER_ENVIRONMENT_UPDATES_TLAS);
}

void RenderEnvironment::SetTLAS(Handle<TLAS> &&tlas)
{
    m_tlas = std::move(tlas);

    InitObject(m_tlas);

    m_update_marker.fetch_or(RENDER_ENVIRONMENT_UPDATES_TLAS);
}

void RenderEnvironment::Init()
{
    if (IsInitCalled()) {
        return;
    }
    
    EngineComponentBase::Init();

    m_particle_system = CreateObject<ParticleSystem>();
    InitObject(m_particle_system);
    
    if (m_tlas) {
        m_rt_radiance.SetTLAS(m_tlas);
        m_rt_radiance.Create();

        m_probe_system.SetTLAS(m_tlas);
        m_probe_system.Init();

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

    OnTeardown([this]() {
        SetReady(false);

        m_particle_system.Reset();
            
        if (m_has_rt_radiance) {
            m_rt_radiance.Destroy();

            m_has_rt_radiance = false;
        }

        if (m_has_ddgi_probes) {
            m_probe_system.Destroy();

            m_has_ddgi_probes = false;
        }

        const auto update_marker_value = m_update_marker.load();

        PUSH_RENDER_COMMAND(RemoveAllRenderComponents, std::move(m_render_components));

        if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS) {
            std::lock_guard guard(m_render_component_mutex);

            m_render_components_pending_addition.Clear();
            m_render_components_pending_removal.Clear();
        }

        m_update_marker.fetch_and(~RENDER_ENVIRONMENT_UPDATES_CONTAINERS);

        HYP_SYNC_RENDER();
    });
}

void RenderEnvironment::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    m_global_timer += delta;

    std::lock_guard guard(m_render_component_mutex);

    for (const auto &it : m_render_components) {
        for (const auto &component_tag_pair : it.second) {
            component_tag_pair.second->ComponentUpdate(delta);
        }
    }
}

void RenderEnvironment::ApplyTLASUpdates(Frame *frame, RTUpdateStateFlags flags)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    AssertThrow(g_engine->GetConfig().Get(CONFIG_RT_SUPPORTED));
    
    if (m_has_rt_radiance) {
        m_rt_radiance.ApplyTLASUpdates(flags);
    }

    if (m_has_ddgi_probes) {
        m_probe_system.ApplyTLASUpdates(flags);
    }
}

void RenderEnvironment::RenderRTRadiance(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    AssertThrow(g_engine->GetConfig().Get(CONFIG_RT_SUPPORTED));
    
    if (m_has_rt_radiance) {
        m_rt_radiance.Render(frame);
    }
}

void RenderEnvironment::RenderDDGIProbes(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    AssertThrow(g_engine->GetConfig().Get(CONFIG_RT_SUPPORTED));
    
    if (m_has_ddgi_probes) {
        m_probe_system.RenderProbes(frame);
        m_probe_system.ComputeIrradiance(frame);

        if (g_engine->GetConfig().Get(CONFIG_RT_GI_DEBUG_PROBES)) {
            for (const auto &probe : m_probe_system.GetProbes()) {
                g_engine->GetImmediateMode().Sphere(probe.position);
            }
        }
    }
}

void RenderEnvironment::RenderComponents(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    m_current_enabled_render_components_mask = m_next_enabled_render_components_mask;

    const RenderEnvironmentUpdates update_marker_value = m_update_marker.load();
    RenderEnvironmentUpdates inverse_mask = 0;

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_ENTITIES) {
        inverse_mask |= RENDER_ENVIRONMENT_UPDATES_ENTITIES;
    }

    for (const auto &it : m_render_components) {
        for (const auto &component_tag_pair : it.second) {
            m_next_enabled_render_components_mask |= 1u << UInt32(component_tag_pair.second->GetName());

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
                        name.LookupString().Data()
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
                RenderComponentName component_type = RENDER_COMPONENT_INVALID;

                auto &items_map = components_it->second;
                
                const auto item_it = items_map.Find(name);

                if (item_it != items_map.End()) {
                    const UniquePtr<RenderComponentBase> &item = item_it->second;

                    if (item != nullptr) {
                        component_type = item->GetName();

                        item->ComponentRemoved();
                    }

                    items_map.Erase(item_it);
                }

                if (items_map.Empty()) {
                    if (component_type != RENDER_COMPONENT_INVALID) {
                        m_next_enabled_render_components_mask &= ~(1u << UInt32(component_type));
                    }

                    m_render_components.Erase(components_it);
                }
            }
        }

        m_render_components_pending_removal.Clear();
    }

    if (g_engine->GetConfig().Get(CONFIG_RT_ENABLED)) {
        if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_TLAS) {
            inverse_mask |= RENDER_ENVIRONMENT_UPDATES_TLAS;

            if (m_tlas) {
                if (m_has_rt_radiance) {
                    m_rt_radiance.Destroy();
                }

                if (m_has_ddgi_probes) {
                    m_probe_system.Destroy();
                }
                
                m_probe_system.SetTLAS(m_tlas);
                m_rt_radiance.SetTLAS(m_tlas);

                m_rt_radiance.Create();
                m_probe_system.Init();

                m_has_rt_radiance = true;
                m_has_ddgi_probes = true;

                HYP_SYNC_RENDER();
            } else {
                if (m_has_rt_radiance) {
                    m_rt_radiance.Destroy();
                }

                if (m_has_ddgi_probes) {
                    m_probe_system.Destroy();
                }

                m_has_rt_radiance = false;
                m_has_ddgi_probes = false;

                HYP_SYNC_RENDER();
            }
        }

        // for RT, we may need to perform resizing of buffers, and thus modification of descriptor sets
        if (const auto &tlas = m_scene->GetTLAS()) {
            RTUpdateStateFlags update_state_flags = renderer::RT_UPDATE_STATE_FLAGS_NONE;

            tlas->UpdateRender(
                frame,
                update_state_flags
            );

            if (update_state_flags) {
                ApplyTLASUpdates(frame, update_state_flags);
            }
        }
    }

    if (update_marker_value) {
        m_update_marker.fetch_and(~inverse_mask);
    }

    ++m_frame_counter;
}

ID<Scene> RenderEnvironment::GetSceneID() const
{
    if (!m_scene) {
        return ID<Scene>();
    }

    return m_scene->GetID();
}

} // namespace hyperion::v2
