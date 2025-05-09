/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/ReflectionProbeRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderEnvProbe.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <system/AppContext.hpp>

#include <scene/camera/PerspectiveCamera.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

namespace hyperion {

ReflectionProbeRenderer::ReflectionProbeRenderer(
    Name name,
    const TResourceHandle<EnvProbeRenderResource> &env_probe_resource_handle
) : RenderSubsystem(name),
    m_env_probe_resource_handle(env_probe_resource_handle),
    m_last_visibility_state(false)
{
}

ReflectionProbeRenderer::~ReflectionProbeRenderer()
{
}

void ReflectionProbeRenderer::Init()
{
    m_env_probe_resource_handle->EnqueueBind();
}

// called from game thread
void ReflectionProbeRenderer::InitGame()
{
    Threads::AssertOnThread(g_game_thread);
}

void ReflectionProbeRenderer::OnRemoved()
{
    m_env_probe_resource_handle->EnqueueUnbind();
}

void ReflectionProbeRenderer::OnUpdate(GameCounter::TickUnit delta)
{
}

void ReflectionProbeRenderer::OnRender(FrameBase *frame)
{
    Threads::AssertOnThread(g_render_thread);

    if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.debug.reflection_probes").ToBool()) {
        g_engine->GetDebugDrawer()->ReflectionProbe(
            m_env_probe_resource_handle->GetBufferData().world_position.GetXYZ(),
            0.5f,
            *m_env_probe_resource_handle->GetEnvProbe()
        );
    }

    if (!m_env_probe_resource_handle->GetEnvProbe()->NeedsRender()) {
        return;
    }

    m_env_probe_resource_handle->Render(frame);

    HYP_LOG(Rendering, Debug, "Rendering ReflectionProbe #{} (type: {})",
        m_env_probe_resource_handle->GetEnvProbe()->GetID().Value(),
        (uint32)m_env_probe_resource_handle->GetEnvProbe()->GetEnvProbeType()
    );
}

void ReflectionProbeRenderer::OnComponentIndexChanged(RenderSubsystem::Index new_index, RenderSubsystem::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

} // namespace hyperion
