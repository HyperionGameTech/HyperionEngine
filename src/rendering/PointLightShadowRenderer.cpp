/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/PointLightShadowRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderEnvProbe.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <scene/Light.hpp>
#include <scene/Scene.hpp>
#include <scene/EnvProbe.hpp>

#include <scene/camera/Camera.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

PointLightShadowRenderer::PointLightShadowRenderer(
    Name name,
    const Handle<Scene> &parent_scene,
    const Handle<Light> &light,
    const Vec2u &extent
) : RenderSubsystem(name),
    m_parent_scene(parent_scene),
    m_light(light),
    m_extent(extent)
{
}

PointLightShadowRenderer::~PointLightShadowRenderer() = default;

void PointLightShadowRenderer::Init()
{
    AssertThrow(m_parent_scene.IsValid());

    if (!InitObject(m_light)) {
        HYP_LOG(Shadows, Warning, "Point shadow renderer attached to invalid Light");

        return;
    }

    m_aabb = m_light->GetAABB();

    m_env_probe = CreateObject<EnvProbe>(
        m_parent_scene,
        m_aabb,
        m_extent,
        ENV_PROBE_TYPE_SHADOW
    );

    InitObject(m_env_probe);

    m_light->SetShadowMapIndex(m_env_probe->GetID().ToIndex());
    m_env_probe->EnqueueBind();
    m_last_visibility_state = true;

    AssertThrow(m_parent_scene->GetCamera().IsValid());
    AssertThrow(m_parent_scene->GetCamera()->IsReady());

    m_camera_resource_handle = ResourceHandle(m_parent_scene->GetCamera()->GetRenderResource());
}

// called from game thread
void PointLightShadowRenderer::InitGame()
{
    Threads::AssertOnThread(g_game_thread);

    AssertThrow(m_env_probe.IsValid());
}

void PointLightShadowRenderer::OnRemoved()
{
    if (m_env_probe.IsValid()) {
        m_env_probe->EnqueueUnbind();
    }

    m_env_probe.Reset();
}

void PointLightShadowRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    AssertThrow(m_env_probe.IsValid());
    AssertThrow(m_light.IsValid());

    const BoundingBox &env_probe_aabb = m_env_probe->GetAABB();
    const BoundingBox light_aabb = m_light->GetAABB();

    if (env_probe_aabb != light_aabb) {
        m_env_probe->SetAABB(light_aabb);
    }

    m_env_probe->Update(delta);
}

void PointLightShadowRenderer::OnRender(Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    if (!m_env_probe.IsValid() || !m_light.IsValid()) {
        HYP_LOG(Shadows, Warning, "Point shadow renderer attached to invalid Light or EnvProbe");

        return;
    }

    if (!m_camera_resource_handle) {
        return;
    }
   
    CameraRenderResource &camera_render_resource = static_cast<CameraRenderResource &>(*m_camera_resource_handle);

    LightRenderResource &light_render_resource = m_light->GetRenderResource();

    if (light_render_resource.GetVisibilityBits().Test(camera_render_resource.GetCamera()->GetID().ToIndex())) {
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

void PointLightShadowRenderer::OnComponentIndexChanged(RenderSubsystem::Index new_index, RenderSubsystem::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

} // namespace hyperion
