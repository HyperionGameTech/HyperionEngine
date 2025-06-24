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
#include <rendering/Renderer.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <scene/ecs/EntityManager.hpp>
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

PointLightShadowRenderer::PointLightShadowRenderer(Name name, const Handle<Scene>& parent_scene, const Handle<Light>& light, const Vec2u& extent)
    : RenderSubsystem(name),
      m_parent_scene(parent_scene),
      m_light(light),
      m_extent(extent)
{
}

PointLightShadowRenderer::~PointLightShadowRenderer()
{
    HYP_SYNC_RENDER(); // wait for render commands to finish
}

void PointLightShadowRenderer::Init()
{
    AssertThrow(m_parent_scene.IsValid());
    AssertThrow(m_parent_scene->IsReady());

    RenderShadowMap* shadow_map = g_render_global_state->ShadowMapAllocator->AllocateShadowMap(
        SMT_OMNI,
        SMF_VSM,
        m_extent);
    AssertThrowMsg(shadow_map != nullptr, "Failed to allocate shadow map");

    m_shadow_map = TResourceHandle<RenderShadowMap>(*shadow_map);
    m_aabb = m_light->GetAABB();

    m_env_probe = m_parent_scene->GetEntityManager()->AddEntity<EnvProbe>(EPT_SHADOW, m_aabb, m_extent);
    InitObject(m_env_probe);

    m_env_probe->GetRenderResource().SetShadowMap(TResourceHandle<RenderShadowMap>(m_shadow_map));

    if (m_light.IsValid())
    {
        InitObject(m_light);

        m_light->GetRenderResource().SetShadowMap(TResourceHandle<RenderShadowMap>(m_shadow_map));
    }
}

void PointLightShadowRenderer::OnRemoved()
{
    if (m_light.IsValid())
    {
        m_light->GetRenderResource().SetShadowMap(TResourceHandle<RenderShadowMap>());
    }

    // if (m_env_probe.IsValid())
    // {
    //     m_env_probe->GetRenderResource().SetShadowMap(TResourceHandle<RenderShadowMap>());

    //     m_env_probe->GetRenderResource().DecRef(); // temp testing
    // }

    // m_env_probe.Reset();

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

void PointLightShadowRenderer::OnUpdate(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    AssertThrow(m_env_probe.IsValid());
    AssertThrow(m_light.IsValid());

    const BoundingBox& env_probe_aabb = m_env_probe->GetAABB();
    const BoundingBox light_aabb = m_light->GetAABB();

    if (env_probe_aabb != light_aabb)
    {
        m_env_probe->SetAABB(light_aabb);
    }
}

} // namespace hyperion
