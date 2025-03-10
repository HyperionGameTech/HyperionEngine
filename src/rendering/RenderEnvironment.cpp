/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Engine.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/DirectionalLightShadowRenderer.hpp>
#include <rendering/RenderScene.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <system/AppContext.hpp>

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {


#pragma region Render commands

struct RENDER_COMMAND(RemoveAllRenderSubsystems) : renderer::RenderCommand
{
    TypeMap<FlatMap<Name, RC<RenderSubsystem>>> render_subsystems;

    RENDER_COMMAND(RemoveAllRenderSubsystems)(TypeMap<FlatMap<Name, RC<RenderSubsystem>>> &&render_subsystems)
        : render_subsystems(std::move(render_subsystems))
    {
    }

    virtual RendererResult operator()() override
    {
        RendererResult result;

        for (auto &it : render_subsystems) {
            for (auto &component_tag_pair : it.second) {
                if (component_tag_pair.second == nullptr) {
                    DebugLog(
                        LogType::Warn,
                        "RenderSubsystem with name %s was null, skipping...\n",
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
      m_current_enabled_render_subsystems_mask(0),
      m_next_enabled_render_subsystems_mask(0),
      m_ddgi(DDGIInfo {
          .aabb = {{-50.0f, -5.0f, -50.0f}, {50.0f, 60.0f, 50.0f}}
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
    }

    if (m_has_ddgi_probes) {
        m_ddgi.Destroy();
    }

    m_enabled_render_subsystems = { };

    PUSH_RENDER_COMMAND(RemoveAllRenderSubsystems, std::move(m_render_subsystems));

    SafeRelease(std::move(m_tlas));

    HYP_SYNC_RENDER();
}

void RenderEnvironment::SetTLAS(const TLASRef &tlas)
{
    m_tlas = tlas;

    AddUpdateMarker(RENDER_ENVIRONMENT_UPDATES_TLAS, ThreadType::THREAD_TYPE_RENDER);
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
    Threads::AssertOnThread(g_game_thread);

    AssertReady();

    if (GetUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_GAME)) {
        m_enabled_render_subsystems[ThreadType::THREAD_TYPE_GAME].Clear();

        bool all_ready = true;

        Mutex::Guard guard(m_render_subsystems_mutex);

        for (auto &render_subsystems_it : m_render_subsystems) {
            for (SizeType index = 0; index < render_subsystems_it.second.Size(); index++) {
                const auto &pair = render_subsystems_it.second.AtIndex(index);

                const Name name = pair.first;
                const RC<RenderSubsystem> &render_subsystem = pair.second;

                if (!render_subsystem->IsInitialized()) {
                    all_ready = false;

                    continue;
                }

                m_enabled_render_subsystems[ThreadType::THREAD_TYPE_GAME].PushBack(render_subsystem);
            }
        }

        if (all_ready) {
            RemoveUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_GAME);
        }
    }

    // @TODO: Use double buffering, or have another atomic flag for game thread and update an internal array to match m_render_subsystems
    for (const RC<RenderSubsystem> &render_subsystem : m_enabled_render_subsystems[ThreadType::THREAD_TYPE_GAME]) {
        AssertThrow(render_subsystem->IsInitialized());

        render_subsystem->ComponentUpdate(delta);
    }
}

void RenderEnvironment::ApplyTLASUpdates(Frame *frame, RTUpdateStateFlags flags)
{
    Threads::AssertOnThread(g_render_thread);
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
    Threads::AssertOnThread(g_render_thread);
    AssertReady();
    
    if (m_has_rt_radiance) {
        m_rt_radiance->Render(frame);
    }
}

void RenderEnvironment::RenderDDGIProbes(Frame *frame)
{
    Threads::AssertOnThread(g_render_thread);
    AssertReady();

    AssertThrow(g_engine->GetGPUDevice()->GetFeatures().IsRaytracingSupported());
    
    if (m_has_ddgi_probes) {
        const DirectionalLightShadowRenderer *shadow_map_renderer = GetRenderSubsystem<DirectionalLightShadowRenderer>();

        m_ddgi.RenderProbes(frame);
        m_ddgi.ComputeIrradiance(frame);

        // if (g_engine->GetConfig().Get(CONFIG_RT_GI_DEBUG_PROBES).GetBool()) {
        //     for (const Probe &probe : m_ddgi.GetProbes()) {
        //         g_engine->GetDebugDrawer()->Sphere(probe.position);
        //     }
        // }
    }
}

void RenderEnvironment::RenderSubsystems(Frame *frame)
{
    Threads::AssertOnThread(g_render_thread);
    AssertReady();

    if (GetUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_RENDER)) {
        m_enabled_render_subsystems[ThreadType::THREAD_TYPE_RENDER].Clear();

        Mutex::Guard guard(m_render_subsystems_mutex);

        for (auto &render_subsystems_it : m_render_subsystems) {
            for (SizeType index = 0; index < render_subsystems_it.second.Size(); index++) {
                const auto &pair = render_subsystems_it.second.AtIndex(index);

                const Name name = pair.first;
                const RC<RenderSubsystem> &render_subsystem = pair.second;
                
                AssertThrow(render_subsystem != nullptr);

                render_subsystem->SetComponentIndex(index);

                if (!render_subsystem->IsInitialized()) {
                    render_subsystem->ComponentInit();
                }

                m_enabled_render_subsystems[ThreadType::THREAD_TYPE_RENDER].PushBack(render_subsystem);
            }
        }

        RemoveUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_RENDER);
    }

    m_current_enabled_render_subsystems_mask = m_next_enabled_render_subsystems_mask;

    for (const RC<RenderSubsystem> &render_subsystem : m_enabled_render_subsystems[ThreadType::THREAD_TYPE_RENDER]) {
        render_subsystem->ComponentRender(frame);
    }

    //if (m_update_marker.Get(MemoryOrder::ACQUIRE) & RENDER_ENVIRONMENT_UPDATES_TLAS) {
        // for RT, we may need to perform resizing of buffers, and thus modification of descriptor sets

    if (!m_rt_initialized) {
        InitializeRT();
    }

    AssertThrow(m_scene != nullptr);
    AssertThrow(m_scene->IsReady());

    if (const TLASRef &tlas = m_scene->GetTLAS()) {
        RTUpdateStateFlags update_state_flags = renderer::RT_UPDATE_STATE_FLAGS_NONE;

        tlas->UpdateStructure(
            g_engine->GetGPUInstance(),
            update_state_flags
        );

        if (update_state_flags) {
            ApplyTLASUpdates(frame, update_state_flags);
        }

        RemoveUpdateMarker(RENDER_ENVIRONMENT_UPDATES_TLAS, ThreadType::THREAD_TYPE_RENDER);
    }

    ++m_frame_counter;
}

TypeID RenderEnvironment::GetRenderSubsystemTypeID(const HypClass *hyp_class)
{
    AssertThrow(hyp_class != nullptr);
    
    static const HypClass *render_subsystem_base_class = RenderSubsystem::Class();

    while (hyp_class->GetParent() != nullptr && hyp_class->GetParent() != render_subsystem_base_class) {
        hyp_class = hyp_class->GetParent();
    }

    return hyp_class->GetTypeID();
}

void RenderEnvironment::AddRenderSubsystem(TypeID type_id, const RC<RenderSubsystem> &render_subsystem)
{
    struct RENDER_COMMAND(AddRenderSubsystem) : renderer::RenderCommand
    {
        WeakHandle<RenderEnvironment>   render_environment_weak;
        TypeID                          type_id;
        RC<RenderSubsystem>             render_subsystem;

        RENDER_COMMAND(AddRenderSubsystem)(const WeakHandle<RenderEnvironment> &render_environment_weak, TypeID type_id, const RC<RenderSubsystem> &render_subsystem)
            : render_environment_weak(render_environment_weak),
              type_id(type_id),
              render_subsystem(render_subsystem)
        {
        }
        
        virtual ~RENDER_COMMAND(AddRenderSubsystem)() override = default;

        virtual RendererResult operator()() override
        {
            Handle<RenderEnvironment> render_environment = render_environment_weak.Lock();
            
            if (!render_environment) {
                return HYP_MAKE_ERROR(RendererError, "RenderEnvironment is null");
            }

            const Name name = render_subsystem->GetName();

            Mutex::Guard guard(render_environment->m_render_subsystems_mutex);

            const auto components_it = render_environment->m_render_subsystems.Find(type_id);

            if (components_it != render_environment->m_render_subsystems.End()) {
                const auto name_it = components_it->second.Find(name);

                AssertThrowMsg(
                    name_it == components_it->second.End(),
                    "Render component with name %s already exists! Name must be unique.\n",
                    name.LookupString()
                );

                components_it->second.Set(name, std::move(render_subsystem));
            } else {
                render_environment->m_render_subsystems.Set(type_id, { { name, std::move(render_subsystem) } });
            }

            render_environment->AddUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_RENDER);
            render_environment->AddUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_GAME);

            HYPERION_RETURN_OK;
        }
    };

    AssertThrow(render_subsystem != nullptr);

    render_subsystem->SetParent(this);

    PUSH_RENDER_COMMAND(AddRenderSubsystem, WeakHandleFromThis(), type_id, render_subsystem);
}

void RenderEnvironment::RemoveRenderSubsystem(TypeID type_id, const HypClass *hyp_class, Name name)
{
    struct RENDER_COMMAND(RemoveRenderSubsystem) : renderer::RenderCommand
    {
        WeakHandle<RenderEnvironment>   render_environment_weak;
        TypeID                          type_id;
        const HypClass                  *hyp_class;
        Name                            name;

        RENDER_COMMAND(RemoveRenderSubsystem)(const WeakHandle<RenderEnvironment> &render_environment_weak, TypeID type_id, const HypClass *hyp_class, Name name)
            : render_environment_weak(render_environment_weak),
              type_id(type_id),
              hyp_class(hyp_class),
              name(name)
        {
        }
        
        virtual ~RENDER_COMMAND(RemoveRenderSubsystem)() override = default;

        virtual RendererResult operator()() override
        {
            Handle<RenderEnvironment> render_environment = render_environment_weak.Lock();
            
            if (!render_environment) {
                return HYP_MAKE_ERROR(RendererError, "RenderEnvironment is null");
            }

            const bool skip_instance_class_check = hyp_class->GetTypeID() == type_id;

            Mutex::Guard guard(render_environment->m_render_subsystems_mutex);

            const auto components_it = render_environment->m_render_subsystems.Find(type_id);

            if (components_it != render_environment->m_render_subsystems.End()) {
                render_environment->AddUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_RENDER);
                render_environment->AddUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_GAME);

                if (name.IsValid()) {
                    const auto name_it = components_it->second.Find(name);

                    if (name_it != components_it->second.End()) {
                        const RC<RenderSubsystem> &render_subsystem = name_it->second;

                        if (skip_instance_class_check || render_subsystem->IsInstanceOf(hyp_class)) {
                            if (render_subsystem && render_subsystem->IsInitialized()) {
                                render_subsystem->ComponentRemoved();
                            }

                            components_it->second.Erase(name_it);
                        }
                    }
                } else {
                    for (auto it = components_it->second.Begin(); it != components_it->second.End();) {
                        const RC<RenderSubsystem> &render_subsystem = it->second;

                        if (skip_instance_class_check || render_subsystem->IsInstanceOf(hyp_class)) {
                            if (render_subsystem && render_subsystem->IsInitialized()) {
                                render_subsystem->ComponentRemoved();

                                it = components_it->second.Erase(it);

                                continue;
                            }
                        }

                        ++it;
                    }
                }

                if (components_it->second.Empty()) {
                    render_environment->m_render_subsystems.Erase(components_it);
                }
            }

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(RemoveRenderSubsystem, WeakHandleFromThis(), type_id, hyp_class, name);
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
