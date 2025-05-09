/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/PointLightShadowRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderShadowMap.hpp>

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
    const Handle<Scene> &parent_scene,
    const TResourceHandle<LightRenderResource> &light_render_resource_handle,
    const Vec2u &extent
) : RenderSubsystem(name),
    m_parent_scene(parent_scene),
    m_light_render_resource_handle(light_render_resource_handle),
    m_extent(extent)
{
}

PointLightShadowRenderer::~PointLightShadowRenderer() = default;

void PointLightShadowRenderer::Init()
{
    AssertThrow(m_parent_scene.IsValid());
    AssertThrow(m_parent_scene->IsReady());

    AssertThrow(m_light_render_resource_handle);

    ShadowMapRenderResource *shadow_map_render_resource = m_parent_scene->GetWorld()->GetRenderResource().GetShadowMapManager()->AllocateShadowMap(
        ShadowMapType::POINT_SHADOW_MAP,
        ShadowMapFilterMode::STANDARD,
        m_extent
    );
    AssertThrowMsg(shadow_map_render_resource != nullptr, "Failed to allocate shadow map");

    m_shadow_map_render_resource_handle = TResourceHandle<ShadowMapRenderResource>(*shadow_map_render_resource);

    m_aabb = BoundingBox(
        m_light_render_resource_handle->GetBufferData().aabb_min.GetXYZ(),
        m_light_render_resource_handle->GetBufferData().aabb_max.GetXYZ()
    );

    m_env_probe = CreateObject<EnvProbe>(
        m_parent_scene,
        m_aabb,
        m_extent,
        ENV_PROBE_TYPE_SHADOW
    );

    InitObject(m_env_probe);

    m_env_probe->GetRenderResource().SetShadowMapResourceHandle(TResourceHandle<ShadowMapRenderResource>(m_shadow_map_render_resource_handle));
    m_env_probe->EnqueueBind();

    m_light_render_resource_handle->SetShadowMapResourceHandle(TResourceHandle<ShadowMapRenderResource>(m_shadow_map_render_resource_handle));

    m_last_visibility_state = true;

    m_scene_render_resource_handle = TResourceHandle<SceneRenderResource>(m_parent_scene->GetRenderResource());
}

// called from game thread
void PointLightShadowRenderer::InitGame()
{
    Threads::AssertOnThread(g_game_thread);

    AssertThrow(m_env_probe.IsValid());
}

void PointLightShadowRenderer::OnRemoved()
{
    if (m_shadow_map_render_resource_handle) {
        ShadowMapRenderResource *shadow_map_render_resource = m_shadow_map_render_resource_handle.Get();
        
        m_shadow_map_render_resource_handle.Reset();

        if (m_parent_scene) {
            if (!m_parent_scene->GetWorld()->GetRenderResource().GetShadowMapManager()->FreeShadowMap(shadow_map_render_resource)) {
                HYP_LOG(Shadows, Error, "Failed to free shadow map!");
            }
        } else {
            HYP_LOG(Shadows, Warning, "Point shadow renderer attached to invalid Scene");

            FreeResource(shadow_map_render_resource);
        }
    }

    if (m_light_render_resource_handle) {
        m_light_render_resource_handle->SetShadowMapResourceHandle(TResourceHandle<ShadowMapRenderResource>());
    }

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

    AssertThrow(m_light_render_resource_handle);

    const BoundingBox &env_probe_aabb = m_env_probe->GetAABB();
    const BoundingBox light_aabb = m_light_render_resource_handle->GetLight()->GetAABB();

    if (env_probe_aabb != light_aabb) {
        m_env_probe->SetAABB(light_aabb);
    }

    m_env_probe->Update(delta);
}

void PointLightShadowRenderer::OnRender(FrameBase *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    if (!m_env_probe.IsValid() || !m_light_render_resource_handle) {
        HYP_LOG(Shadows, Warning, "Point shadow renderer attached to invalid Light or EnvProbe");

        return;
    }

    const TResourceHandle<CameraRenderResource> &camera_render_resource_handle = m_scene_render_resource_handle->GetCameraRenderResourceHandle();

    if (!camera_render_resource_handle) {
        return;
    }

    const ID<Camera> camera_id = camera_render_resource_handle->GetCamera()->GetID();

    if (m_light_render_resource_handle->GetVisibilityBits().Test(camera_id.ToIndex())) {
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
