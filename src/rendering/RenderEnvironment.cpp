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

RenderEnvironment::RenderEnvironment()
    : m_ddgi(DDGIInfo {
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

    SafeRelease(std::move(m_top_level_acceleration_structures));
}

void RenderEnvironment::Initialize()
{
    m_particle_system = CreateObject<ParticleSystem>();
    InitObject(m_particle_system);

    m_gaussian_splatting = CreateObject<GaussianSplatting>();
    InitObject(m_gaussian_splatting);

    /// FIXME: GetCurrentView() should not be used here
    // m_rt_radiance = MakeUnique<RaytracingReflections>(
    //     RaytracingReflectionsConfig::FromConfig(),
    //     g_engine->GetCurrentView()->GetGBuffer());

    if (g_render_backend->GetRenderConfig().IsRaytracingSupported()
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
}

void RenderEnvironment::ApplyTLASUpdates(FrameBase* frame, RTUpdateStateFlags flags)
{
    Threads::AssertOnThread(g_render_thread);

    static const bool is_raytracing_supported = g_render_backend->GetRenderConfig().IsRaytracingSupported();
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

    if (m_has_rt_radiance)
    {
        m_rt_radiance->Render(frame, render_setup);
    }
}

void RenderEnvironment::RenderDDGIProbes(FrameBase* frame, const RenderSetup& render_setup)
{
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(g_render_backend->GetRenderConfig().IsRaytracingSupported());

    if (m_has_ddgi_probes)
    {
        m_ddgi.Render(frame, render_setup);
    }
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
        m_top_level_acceleration_structures[frame_index] = g_render_backend->MakeTLAS();
        m_top_level_acceleration_structures[frame_index]->AddBLAS(blas);

        DeferCreate(m_top_level_acceleration_structures[frame_index]);
    }

    return true;
}

} // namespace hyperion
