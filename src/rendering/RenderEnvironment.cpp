/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Engine.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/DirectionalLightShadowRenderer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <system/AppContext.hpp>

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <scene/Mesh.hpp>
#include <scene/Material.hpp>

#include <util/MeshBuilder.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(RemoveAllRenderSubsystems)
    : renderer::RenderCommand
{
    TypeMap<FlatMap<Name, RC<RenderSubsystem>>> render_subsystems;

    RENDER_COMMAND(RemoveAllRenderSubsystems)(TypeMap<FlatMap<Name, RC<RenderSubsystem>>>&& render_subsystems)
        : render_subsystems(std::move(render_subsystems))
    {
    }

    virtual RendererResult operator()() override
    {
        RendererResult result;

        for (auto& it : render_subsystems)
        {
            for (auto& component_tag_pair : it.second)
            {
                if (component_tag_pair.second == nullptr)
                {
                    DebugLog(
                        LogType::Warn,
                        "RenderSubsystem with name %s was null, skipping...\n",
                        component_tag_pair.first.LookupString());

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
    : HypObject(),
      m_current_enabled_render_subsystems_mask(0),
      m_next_enabled_render_subsystems_mask(0),
      m_ddgi(DDGIInfo {
          .aabb = { { -45.0f, -5.0f, -45.0f }, { 45.0f, 60.0f, 45.0f } } }),
      m_has_rt_radiance(false),
      m_has_ddgi_probes(false),
      m_rt_initialized(false)
{
}

RenderEnvironment::~RenderEnvironment()
{
    m_particle_system.Reset();

    m_gaussian_splatting.Reset();

    if (m_has_rt_radiance)
    {
        m_rt_radiance->Destroy();
        m_rt_radiance.Reset();
    }

    if (m_has_ddgi_probes)
    {
        m_ddgi.Destroy();
    }

    m_enabled_render_subsystems = {};

    PUSH_RENDER_COMMAND(RemoveAllRenderSubsystems, std::move(m_render_subsystems));

    SafeRelease(std::move(m_top_level_acceleration_structures));

    HYP_SYNC_RENDER();
}

void RenderEnvironment::Init()
{
    if (IsInitCalled())
    {
        return;
    }

    HypObject::Init();

    m_particle_system = CreateObject<ParticleSystem>();
    InitObject(m_particle_system);

    m_gaussian_splatting = CreateObject<GaussianSplatting>();
    InitObject(m_gaussian_splatting);

    // @TODO Move to DeferredRenderer
    m_rt_radiance = MakeUnique<RTRadianceRenderer>(
        RTRadianceConfig::FromConfig(),
        g_engine->GetCurrentView()->GetGBuffer());

    if (g_rendering_api->GetRenderConfig().IsRaytracingSupported()
        && g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool())
    {
        CreateTopLevelAccelerationStructures();

        m_rt_radiance->SetTopLevelAccelerationStructures(m_top_level_acceleration_structures);
        m_rt_radiance->Create();

        m_ddgi.SetTopLevelAccelerationStructures(m_top_level_acceleration_structures);
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

    if (GetUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_GAME))
    {
        m_enabled_render_subsystems[ThreadType::THREAD_TYPE_GAME].Clear();

        bool all_ready = true;

        Mutex::Guard guard(m_render_subsystems_mutex);

        for (auto& render_subsystems_it : m_render_subsystems)
        {
            for (SizeType index = 0; index < render_subsystems_it.second.Size(); index++)
            {
                const auto& pair = render_subsystems_it.second.AtIndex(index);

                const Name name = pair.first;
                const RC<RenderSubsystem>& render_subsystem = pair.second;

                if (!render_subsystem->IsInitialized())
                {
                    all_ready = false;

                    continue;
                }

                m_enabled_render_subsystems[ThreadType::THREAD_TYPE_GAME].PushBack(render_subsystem);
            }
        }

        if (all_ready)
        {
            RemoveUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_GAME);
        }
    }

    // @TODO: Use double buffering, or have another atomic flag for game thread and update an internal array to match m_render_subsystems
    for (const RC<RenderSubsystem>& render_subsystem : m_enabled_render_subsystems[ThreadType::THREAD_TYPE_GAME])
    {
        render_subsystem->ComponentUpdate(delta);
    }
}

void RenderEnvironment::ApplyTLASUpdates(FrameBase* frame, RTUpdateStateFlags flags)
{
    Threads::AssertOnThread(g_render_thread);
    AssertReady();

    static const bool is_raytracing_supported = g_rendering_api->GetRenderConfig().IsRaytracingSupported();
    AssertThrow(is_raytracing_supported);

    if (m_has_rt_radiance)
    {
        m_rt_radiance->ApplyTLASUpdates(flags);
    }

    if (m_has_ddgi_probes)
    {
        m_ddgi.ApplyTLASUpdates(flags);
    }
}

void RenderEnvironment::RenderRTRadiance(FrameBase* frame, const RenderSetup& render_setup)
{
    Threads::AssertOnThread(g_render_thread);
    AssertReady();

    if (m_has_rt_radiance)
    {
        m_rt_radiance->Render(frame, render_setup);
    }
}

void RenderEnvironment::RenderDDGIProbes(FrameBase* frame, const RenderSetup& render_setup)
{
    Threads::AssertOnThread(g_render_thread);
    AssertReady();

    AssertThrow(g_rendering_api->GetRenderConfig().IsRaytracingSupported());

    if (m_has_ddgi_probes)
    {
        m_ddgi.RenderProbes(frame, render_setup);
        m_ddgi.ComputeIrradiance(frame, render_setup);
    }
}

void RenderEnvironment::RenderSubsystems(FrameBase* frame, const RenderSetup& render_setup)
{
    Threads::AssertOnThread(g_render_thread);
    AssertReady();

    if (GetUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_RENDER))
    {
        m_enabled_render_subsystems[ThreadType::THREAD_TYPE_RENDER].Clear();

        Mutex::Guard guard(m_render_subsystems_mutex);

        for (auto& render_subsystems_it : m_render_subsystems)
        {
            for (SizeType index = 0; index < render_subsystems_it.second.Size(); index++)
            {
                const auto& pair = render_subsystems_it.second.AtIndex(index);

                const Name name = pair.first;
                const RC<RenderSubsystem>& render_subsystem = pair.second;

                AssertThrow(render_subsystem != nullptr);

                m_enabled_render_subsystems[ThreadType::THREAD_TYPE_RENDER].PushBack(render_subsystem);
            }
        }

        RemoveUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_RENDER);
    }

    m_current_enabled_render_subsystems_mask = m_next_enabled_render_subsystems_mask;

    for (const RC<RenderSubsystem>& render_subsystem : m_enabled_render_subsystems[ThreadType::THREAD_TYPE_RENDER])
    {
        render_subsystem->ComponentRender(frame, render_setup);
    }

    // if (m_update_marker.Get(MemoryOrder::ACQUIRE) & RENDER_ENVIRONMENT_UPDATES_TLAS) {
    //  for RT, we may need to perform resizing of buffers, and thus modification of descriptor sets

    if (!m_rt_initialized)
    {
        InitializeRT();
    }

    if (m_rt_initialized && m_top_level_acceleration_structures[frame->GetFrameIndex()].IsValid())
    {
        RTUpdateStateFlags update_state_flags;

        m_top_level_acceleration_structures[frame->GetFrameIndex()]->UpdateStructure(update_state_flags);

        ApplyTLASUpdates(frame, update_state_flags);
        RemoveUpdateMarker(RENDER_ENVIRONMENT_UPDATES_TLAS, ThreadType::THREAD_TYPE_RENDER);
    }
}

TypeID RenderEnvironment::GetRenderSubsystemTypeID(const HypClass* hyp_class)
{
    AssertThrow(hyp_class != nullptr);

    static const HypClass* render_subsystem_base_class = RenderSubsystem::Class();

    while (hyp_class->GetParent() != nullptr && hyp_class->GetParent() != render_subsystem_base_class)
    {
        hyp_class = hyp_class->GetParent();
    }

    return hyp_class->GetTypeID();
}

void RenderEnvironment::AddRenderSubsystem(TypeID type_id, const RC<RenderSubsystem>& render_subsystem)
{
    struct RENDER_COMMAND(AddRenderSubsystem)
        : renderer::RenderCommand
    {
        WeakHandle<RenderEnvironment> render_environment_weak;
        TypeID type_id;
        RC<RenderSubsystem> render_subsystem;

        RENDER_COMMAND(AddRenderSubsystem)(const WeakHandle<RenderEnvironment>& render_environment_weak, TypeID type_id, const RC<RenderSubsystem>& render_subsystem)
            : render_environment_weak(render_environment_weak),
              type_id(type_id),
              render_subsystem(render_subsystem)
        {
        }

        virtual ~RENDER_COMMAND(AddRenderSubsystem)() override = default;

        virtual RendererResult operator()() override
        {
            Handle<RenderEnvironment> render_environment = render_environment_weak.Lock();

            if (!render_environment)
            {
                return HYP_MAKE_ERROR(RendererError, "RenderEnvironment is null");
            }

            const Name name = render_subsystem->GetName();

            Mutex::Guard guard(render_environment->m_render_subsystems_mutex);

            const auto components_it = render_environment->m_render_subsystems.Find(type_id);

            SizeType index = SizeType(-1);

            if (components_it != render_environment->m_render_subsystems.End())
            {
                auto it = components_it->second.Find(name);

                AssertThrowMsg(
                    it == components_it->second.End(),
                    "Render component with name %s already exists! Name must be unique.\n",
                    name.LookupString());

                it = components_it->second.Set(name, render_subsystem).first;

                index = components_it->second.IndexOf(it);
            }
            else
            {
                auto it = render_environment->m_render_subsystems.Set(type_id, { { name, render_subsystem } }).first;

                index = 0;
            }

            AssertDebug(index != SizeType(-1));

            render_subsystem->SetParent(render_environment.Get());

            if (!render_subsystem->IsInitialized())
            {
                render_subsystem->ComponentInit();
            }

            render_environment->AddUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_RENDER);
            render_environment->AddUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_GAME);

            HYPERION_RETURN_OK;
        }
    };

    AssertThrow(render_subsystem != nullptr);

    PUSH_RENDER_COMMAND(AddRenderSubsystem, WeakHandleFromThis(), type_id, render_subsystem);
}

void RenderEnvironment::RemoveRenderSubsystem(const RenderSubsystem* render_subsystem)
{
    if (!render_subsystem)
    {
        return;
    }

    struct RENDER_COMMAND(RemoveRenderSubsystem)
        : renderer::RenderCommand
    {
        WeakHandle<RenderEnvironment> render_environment_weak;
        RC<RenderSubsystem> render_subsystem;

        RENDER_COMMAND(RemoveRenderSubsystem)(WeakHandle<RenderEnvironment>&& render_environment_weak, RC<RenderSubsystem>&& render_subsystem)
            : render_environment_weak(std::move(render_environment_weak)),
              render_subsystem(std::move(render_subsystem))
        {
        }

        virtual ~RENDER_COMMAND(RemoveRenderSubsystem)() override = default;

        virtual RendererResult operator()() override
        {
            Handle<RenderEnvironment> render_environment = render_environment_weak.Lock();

            if (!render_environment)
            {
                return HYP_MAKE_ERROR(RendererError, "RenderEnvironment is null");
            }

            Mutex::Guard guard(render_environment->m_render_subsystems_mutex);

            const auto components_it = render_environment->m_render_subsystems.Find(GetRenderSubsystemTypeID(render_subsystem->InstanceClass()));

            if (components_it != render_environment->m_render_subsystems.End())
            {
                render_environment->AddUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_RENDER);
                render_environment->AddUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_GAME);

                for (auto it = components_it->second.Begin(); it != components_it->second.End();)
                {
                    if (it->second == render_subsystem)
                    {
                        if (render_subsystem->IsInitialized())
                        {
                            render_subsystem->ComponentRemoved();
                        }

                        render_subsystem->SetParent(nullptr);

                        it = components_it->second.Erase(it);

                        continue;
                    }

                    ++it;
                }

                render_subsystem.Reset();

                if (components_it->second.Empty())
                {
                    render_environment->m_render_subsystems.Erase(components_it);
                }
            }

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(RemoveRenderSubsystem, WeakHandleFromThis(), render_subsystem->RefCountedPtrFromThis());
}

void RenderEnvironment::RemoveRenderSubsystem(TypeID type_id, const HypClass* hyp_class, Name name)
{
    struct RENDER_COMMAND(RemoveRenderSubsystem)
        : renderer::RenderCommand
    {
        WeakHandle<RenderEnvironment> render_environment_weak;
        TypeID type_id;
        const HypClass* hyp_class;
        Name name;

        RENDER_COMMAND(RemoveRenderSubsystem)(const WeakHandle<RenderEnvironment>& render_environment_weak, TypeID type_id, const HypClass* hyp_class, Name name)
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

            if (!render_environment)
            {
                return HYP_MAKE_ERROR(RendererError, "RenderEnvironment is null");
            }

            const bool skip_instance_class_check = hyp_class->GetTypeID() == type_id;

            Mutex::Guard guard(render_environment->m_render_subsystems_mutex);

            const auto components_it = render_environment->m_render_subsystems.Find(type_id);

            if (components_it != render_environment->m_render_subsystems.End())
            {
                render_environment->AddUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_RENDER);
                render_environment->AddUpdateMarker(RENDER_ENVIRONMENT_UPDATES_RENDER_SUBSYSTEMS, ThreadType::THREAD_TYPE_GAME);

                if (name.IsValid())
                {
                    const auto name_it = components_it->second.Find(name);

                    if (name_it != components_it->second.End())
                    {
                        const RC<RenderSubsystem>& render_subsystem = name_it->second;

                        if (skip_instance_class_check || render_subsystem->IsInstanceOf(hyp_class))
                        {
                            if (render_subsystem)
                            {
                                if (render_subsystem->IsInitialized())
                                {
                                    render_subsystem->ComponentRemoved();
                                }

                                render_subsystem->SetParent(nullptr);
                            }

                            components_it->second.Erase(name_it);
                        }
                    }
                }
                else
                {
                    for (auto it = components_it->second.Begin(); it != components_it->second.End();)
                    {
                        const RC<RenderSubsystem>& render_subsystem = it->second;

                        if (skip_instance_class_check || render_subsystem->IsInstanceOf(hyp_class))
                        {
                            if (render_subsystem)
                            {
                                if (render_subsystem->IsInitialized())
                                {
                                    render_subsystem->ComponentRemoved();
                                }

                                render_subsystem->SetParent(nullptr);
                            }

                            it = components_it->second.Erase(it);

                            continue;
                        }

                        ++it;
                    }
                }

                if (components_it->second.Empty())
                {
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
    if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool())
    {
        if (m_has_rt_radiance)
        {
            m_rt_radiance->Destroy();
        }

        if (m_has_ddgi_probes)
        {
            m_ddgi.Destroy();
        }

        m_ddgi.SetTopLevelAccelerationStructures(m_top_level_acceleration_structures);
        m_rt_radiance->SetTopLevelAccelerationStructures(m_top_level_acceleration_structures);

        m_rt_radiance->Create();
        m_ddgi.Init();

        m_has_rt_radiance = true;
        m_has_ddgi_probes = true;
    }
    else
    {
        if (m_has_rt_radiance)
        {
            m_rt_radiance->Destroy();
        }

        if (m_has_ddgi_probes)
        {
            m_ddgi.Destroy();
        }

        m_has_rt_radiance = false;
        m_has_ddgi_probes = false;
    }

    m_rt_initialized = true;
}

bool RenderEnvironment::CreateTopLevelAccelerationStructures()
{
    HYP_SCOPE;

    AssertIsInitCalled();

    if (!g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool())
    {
        return false;
    }

    // @FIXME: Hack solution since TLAS can only be created if it has a non-zero number of BLASes.
    // This whole thing should be reworked
    Handle<Mesh> default_mesh = MeshBuilder::Cube();
    InitObject(default_mesh);

    Handle<Material> default_material = CreateObject<Material>();
    InitObject(default_material);

    BLASRef blas;

    {
        TResourceHandle<RenderMesh> mesh_resource_handle(default_mesh->GetRenderResource());

        blas = mesh_resource_handle->BuildBLAS(default_material);
    }

    DeferCreate(blas);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        m_top_level_acceleration_structures[frame_index] = g_rendering_api->MakeTLAS();
        m_top_level_acceleration_structures[frame_index]->AddBLAS(blas);

        DeferCreate(m_top_level_acceleration_structures[frame_index]);
    }

    return true;
}

} // namespace hyperion
