/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Engine.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/DirectionalLightShadowRenderer.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <core/system/AppContext.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

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

RenderEnvironment::RenderEnvironment()
    : RenderEnvironment(nullptr)
{
}

RenderEnvironment::RenderEnvironment(Scene *scene)
    : HypObject(),
      m_scene(scene),
      m_frame_counter(0),
      m_current_enabled_render_components_mask(0),
      m_next_enabled_render_components_mask(0),
      m_ddgi({
          .aabb = {{-25.0f, -5.0f, -25.0f}, {25.0f, 30.0f, 25.0f}}
      }),
      m_has_rt_radiance(false),
      m_has_ddgi_probes(false),
      m_rt_initialized(false)
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

    m_enabled_render_components.Clear();
    PUSH_RENDER_COMMAND(RemoveAllRenderComponents, std::move(m_render_components));

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
    
    HypObject::Init();

    m_particle_system = CreateObject<ParticleSystem>();
    InitObject(m_particle_system);

    m_gaussian_splatting = CreateObject<GaussianSplatting>();
    InitObject(m_gaussian_splatting);
    
    m_rt_radiance = MakeUnique<RTRadianceRenderer>(
        Vec2u { 1024, 1024 },
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

    SetReady(true);
}

void RenderEnvironment::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertReady();

    // @TODO: Use double buffering, or have another atomic flag for game thread and update an internal array to match m_render_components
    Mutex::Guard guard(m_render_components_mutex);

    for (const auto &it : m_render_components) {
        for (const auto &pair : it.second) {
            const RC<RenderComponentBase> &render_component = pair.second;
            AssertThrow(render_component != nullptr);

            if (!render_component->IsInitialized()) {
                continue;
            }

            render_component->ComponentUpdate(delta);
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

    for (const RC<RenderComponentBase> &render_component : m_enabled_render_components) {
        render_component->ComponentRender(frame);
    }

    if (m_update_marker.Get(MemoryOrder::ACQUIRE) & RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS) {
        m_enabled_render_components.Clear();

        Mutex::Guard guard(m_render_components_mutex);

        for (auto &render_components_it : m_render_components) {
            for (SizeType index = 0; index < render_components_it.second.Size(); index++) {
                const auto &pair = render_components_it.second.AtIndex(index);

                const Name name = pair.first;
                const RC<RenderComponentBase> &render_component = pair.second;
                
                AssertThrow(render_component != nullptr);

                render_component->SetComponentIndex(index);

                if (!render_component->IsInitialized()) {
                    render_component->ComponentInit();
                }

                m_enabled_render_components.PushBack(render_component);
            }
        }

        m_update_marker.BitAnd(~RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS, MemoryOrder::RELEASE);
    }

    //if (m_update_marker.Get(MemoryOrder::ACQUIRE) & RENDER_ENVIRONMENT_UPDATES_TLAS) {
        // for RT, we may need to perform resizing of buffers, and thus modification of descriptor sets

    if (!m_rt_initialized) {
        InitializeRT();
        }
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

        m_update_marker.BitAnd(~RENDER_ENVIRONMENT_UPDATES_TLAS, MemoryOrder::RELEASE);
    //}

    ++m_frame_counter;
}

void RenderEnvironment::AddRenderComponent(TypeID type_id, const RC<RenderComponentBase> &render_component)
{
    struct RENDER_COMMAND(AddRenderComponent) : renderer::RenderCommand
    {
        RenderEnvironment       &render_environment;
        TypeID                  type_id;
        RC<RenderComponentBase> render_component;

        RENDER_COMMAND(AddRenderComponent)(RenderEnvironment &render_environment, TypeID type_id, const RC<RenderComponentBase> &render_component)
            : render_environment(render_environment),
              type_id(type_id),
              render_component(render_component)
        {
            AssertThrow(render_component != nullptr);
        }
        
        virtual ~RENDER_COMMAND(AddRenderComponent)() override = default;

        virtual renderer::Result operator()() override
        {
            const Name name = render_component->GetName();

            Mutex::Guard guard(render_environment.m_render_components_mutex);

            const auto components_it = render_environment.m_render_components.Find(type_id);

            if (components_it != render_environment.m_render_components.End()) {
                const auto name_it = components_it->second.Find(name);

                AssertThrowMsg(
                    name_it == components_it->second.End(),
                    "Render component with name %s already exists! Name must be unique.\n",
                    name.LookupString()
                );

                components_it->second.Set(name, std::move(render_component));
            } else {
                render_environment.m_render_components.Set(type_id, { { name, std::move(render_component) } });
            }

            render_environment.m_update_marker.BitOr(RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS, MemoryOrder::RELEASE);

            HYPERION_RETURN_OK;
        }
    };

    AssertThrow(render_component != nullptr);

    render_component->SetParent(this);

    PUSH_RENDER_COMMAND(AddRenderComponent, *this, type_id, render_component);
}

void RenderEnvironment::RemoveRenderComponent(TypeID type_id, Name name)
{
    struct RENDER_COMMAND(RemoveRenderComponent) : renderer::RenderCommand
    {
        RenderEnvironment       &render_environment;
        TypeID                  type_id;
        Name                    name;

        RENDER_COMMAND(RemoveRenderComponent)(RenderEnvironment &render_environment, TypeID type_id, Name name)
            : render_environment(render_environment),
              type_id(type_id),
              name(name)
        {
        }
        
        virtual ~RENDER_COMMAND(RemoveRenderComponent)() override = default;

        virtual renderer::Result operator()() override
        {
            Mutex::Guard guard(render_environment.m_render_components_mutex);

            const auto components_it = render_environment.m_render_components.Find(type_id);

            if (components_it != render_environment.m_render_components.End()) {
                if (name.IsValid()) {
                    const auto name_it = components_it->second.Find(name);

                    if (name_it != components_it->second.End()) {
                        const RC<RenderComponentBase> &render_component = name_it->second;

                        if (render_component && render_component->IsInitialized()) {
                            render_component->ComponentRemoved();
                        }

                        components_it->second.Erase(name_it);
                    }
                } else {
                    for (const auto &it : components_it->second) {
                        const RC<RenderComponentBase> &render_component = it.second;

                        if (render_component && render_component->IsInitialized()) {
                            render_component->ComponentRemoved();
                        }
                    }

                    components_it->second.Clear();
                }

                if (components_it->second.Empty()) {
                    render_environment.m_render_components.Erase(components_it);
                }
            }

            render_environment.m_update_marker.BitOr(RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS, MemoryOrder::RELEASE);

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(RemoveRenderComponent, *this, type_id, name);
}

void RenderEnvironment::InitializeRT()
{
    if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool()) {
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

    m_rt_initialized = true;
}

} // namespace hyperion
