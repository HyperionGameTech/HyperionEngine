/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/PointLightShadowRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <scene/Light.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/EnvProbe.hpp>

#include <scene/camera/Camera.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

PointLightShadowRenderer::PointLightShadowRenderer(
    Name name,
    const Handle<Scene>& parent_scene,
    const TResourceHandle<RenderLight>& render_light,
    const Vec2u& extent)
    : RenderSubsystem(name),
      m_parent_scene(parent_scene),
      m_render_light(render_light),
      m_extent(extent)
{
}

PointLightShadowRenderer::~PointLightShadowRenderer() = default;

void PointLightShadowRenderer::Init()
{
    AssertThrow(m_parent_scene.IsValid());
    AssertThrow(m_parent_scene->IsReady());

    AssertThrow(m_render_light);

    RenderShadowMap* shadow_map = g_render_global_state->ShadowMapAllocator->AllocateShadowMap(
        ShadowMapType::POINT_SHADOW_MAP,
        ShadowMapFilterMode::VSM,
        m_extent);
    AssertThrowMsg(shadow_map != nullptr, "Failed to allocate shadow map");

    m_shadow_map = TResourceHandle<RenderShadowMap>(*shadow_map);

    m_aabb = BoundingBox(
        m_render_light->GetBufferData().aabb_min.GetXYZ(),
        m_render_light->GetBufferData().aabb_max.GetXYZ());

    m_env_probe = CreateObject<EnvProbe>(
        m_parent_scene,
        m_aabb,
        m_extent,
        EnvProbeType::SHADOW);

    InitObject(m_env_probe);

    m_env_probe->GetRenderResource().SetShadowMap(TResourceHandle<RenderShadowMap>(m_shadow_map));

    // temp
    m_env_probe->GetRenderResource().IncRef();

    m_render_light->SetShadowMap(TResourceHandle<RenderShadowMap>(m_shadow_map));

    m_last_visibility_state = true;

    m_render_scene = TResourceHandle<RenderScene>(m_parent_scene->GetRenderResource());
}

// called from game thread
void PointLightShadowRenderer::InitGame()
{
    Threads::AssertOnThread(g_game_thread);

    AssertThrow(m_env_probe.IsValid());
}

void PointLightShadowRenderer::OnRemoved()
{
    if (m_render_light)
    {
        m_render_light->SetShadowMap(TResourceHandle<RenderShadowMap>());
    }

    if (m_env_probe.IsValid())
    {
        m_env_probe->GetRenderResource().SetShadowMap(TResourceHandle<RenderShadowMap>());

        m_env_probe->GetRenderResource().DecRef(); // temp testing
    }

    m_env_probe.Reset();

    if (m_shadow_map)
    {
        RenderShadowMap* shadow_map = m_shadow_map.Get();

        m_shadow_map.Reset();

        if (!g_render_global_state->ShadowMapAllocator->FreeShadowMap(shadow_map))
        {
            HYP_FAIL("Failed to free shadow map!");
        }
    }
}

void PointLightShadowRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    AssertThrow(m_env_probe.IsValid());

    AssertThrow(m_render_light);

    const BoundingBox& env_probe_aabb = m_env_probe->GetAABB();
    const BoundingBox light_aabb = m_render_light->GetLight()->GetAABB();

    if (env_probe_aabb != light_aabb)
    {
        m_env_probe->SetAABB(light_aabb);
    }

    m_env_probe->Update(delta);
}

void PointLightShadowRenderer::OnRender(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    if (!m_env_probe.IsValid() || !m_render_light)
    {
        HYP_LOG(Shadows, Warning, "Point shadow renderer attached to invalid Light or EnvProbe");

        return;
    }

    // // @FIXME: Should be per-view

    // // if (m_render_light->GetVisibilityBits().Test(camera_id.ToIndex())) {
    // if (!m_last_visibility_state)
    // {
    //     g_engine->GetRenderState()->BindEnvProbe(m_env_probe->GetEnvProbeType(), TResourceHandle<RenderEnvProbe>(m_env_probe->GetRenderResource()));

    //     m_last_visibility_state = true;
    // }

    AssertThrow(m_env_probe->IsReady());

    m_env_probe->GetRenderResource().Render(frame, render_setup);
    // } else {
    //     // No point in keeping it bound if the light is not visible on the screen.
    //     if (m_last_visibility_state) {
    //         g_engine->GetRenderState()->UnbindEnvProbe(m_env_probe->GetEnvProbeType(), &m_env_probe->GetRenderResource());

    //         m_last_visibility_state = false;
    //     }
    // }
}

} // namespace hyperion
