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
    const TResourceHandle<RenderEnvProbe>& env_render_probe)
    : RenderSubsystem(name),
      m_env_render_probe(env_render_probe),
      m_last_visibility_state(false)
{
}

ReflectionProbeRenderer::~ReflectionProbeRenderer()
{
}

void ReflectionProbeRenderer::Init()
{
}

// called from game thread
void ReflectionProbeRenderer::InitGame()
{
    Threads::AssertOnThread(g_game_thread);
}

void ReflectionProbeRenderer::OnRemoved()
{
}

void ReflectionProbeRenderer::OnUpdate(GameCounter::TickUnit delta)
{
}

void ReflectionProbeRenderer::OnRender(FrameBase* frame, const RenderSetup& render_setup)
{
    Threads::AssertOnThread(g_render_thread);

    if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.debug.reflection_probes").ToBool())
    {
        g_engine->GetDebugDrawer()->ReflectionProbe(
            m_env_render_probe->GetBufferData().world_position.GetXYZ(),
            0.5f,
            *m_env_render_probe->GetEnvProbe());
    }

    if (!m_env_render_probe->GetEnvProbe()->NeedsRender())
    {
        return;
    }

    m_env_render_probe->Render(frame, render_setup);

    HYP_LOG(Rendering, Debug, "Rendering ReflectionProbe {} (type: {})",
        m_env_render_probe->GetEnvProbe()->GetID(),
        (uint32)m_env_render_probe->GetEnvProbe()->GetEnvProbeType());
}

} // namespace hyperion
