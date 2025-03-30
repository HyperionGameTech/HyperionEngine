/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/ReflectionProbeRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderEnvProbe.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <system/AppContext.hpp>

#include <scene/EnvProbe.hpp>
#include <scene/camera/PerspectiveCamera.hpp>

#include <Engine.hpp>

namespace hyperion {

ReflectionProbeRenderer::ReflectionProbeRenderer(
    Name name,
    const Handle<Scene> &parent_scene,
    const BoundingBox &aabb
) : RenderSubsystem(name),
    m_parent_scene(parent_scene),
    m_last_visibility_state(false)
{
    m_env_probe = CreateObject<EnvProbe>(
        m_parent_scene,
        aabb,
        Vec2u { 256, 256 },
        ENV_PROBE_TYPE_REFLECTION
    );
}

ReflectionProbeRenderer::~ReflectionProbeRenderer()
{
}

void ReflectionProbeRenderer::Init()
{
    InitObject(m_env_probe);
}

// called from game thread
void ReflectionProbeRenderer::InitGame()
{
    Threads::AssertOnThread(g_game_thread);
}

void ReflectionProbeRenderer::OnRemoved()
{
    AssertThrow(m_env_probe.IsValid());
    m_env_probe->EnqueueUnbind();
}

void ReflectionProbeRenderer::OnUpdate(GameCounter::TickUnit delta)
{
}

void ReflectionProbeRenderer::OnRender(Frame *frame)
{
    Threads::AssertOnThread(g_render_thread);

    if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.debug.reflection_probes").ToBool()) {
        g_engine->GetDebugDrawer()->ReflectionProbe(
            m_env_probe->GetRenderResource().GetBufferData().world_position.GetXYZ(),
            0.5f,
            m_env_probe
        );
    }

    if (m_env_probe->GetRenderResource().GetBufferData().visibility_bits & (1ull << SizeType(m_parent_scene->GetCamera().GetID().ToIndex()))) {
        if (!m_last_visibility_state) {
            g_engine->GetRenderState()->BindEnvProbe(m_env_probe->GetEnvProbeType(), TResourceHandle<EnvProbeRenderResource>(m_env_probe->GetRenderResource()));

            m_last_visibility_state = true;
        }

        m_env_probe->GetRenderResource().Render(frame);
    } else {
        // No point in keeping it bound if the light is not visible on the screen.
        if (m_last_visibility_state) {
            g_engine->GetRenderState()->UnbindEnvProbe(m_env_probe->GetEnvProbeType(), &m_env_probe->GetRenderResource());

            m_last_visibility_state = false;
        }
    }
}

void ReflectionProbeRenderer::OnComponentIndexChanged(RenderSubsystem::Index new_index, RenderSubsystem::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

} // namespace hyperion
