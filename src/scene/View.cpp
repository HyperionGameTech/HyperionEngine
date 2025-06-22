/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/Light.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>
#include <scene/camera/Camera.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/LightmapVolumeComponent.hpp>
#include <scene/ecs/components/ShadowMapComponent.hpp>
#include <scene/ecs/components/ReflectionProbeComponent.hpp>
#include <scene/ecs/components/SkyComponent.hpp>

#include <rendering/RenderView.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/subsystems/sky/SkydomeRenderer.hpp>
#include <rendering/lightmapper/RenderLightmapVolume.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/algorithm/Map.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

// #define HYP_DISABLE_VISIBILITY_CHECK
#define HYP_VISIBILITY_CHECK_DEBUG

namespace hyperion {

#pragma region ViewOutputTarget

ViewOutputTarget::ViewOutputTarget()
    : m_impl(FramebufferRef::Null())
{
}

ViewOutputTarget::ViewOutputTarget(const FramebufferRef& framebuffer)
    : m_impl(framebuffer)
{
}

ViewOutputTarget::ViewOutputTarget(GBuffer* gbuffer)
    : m_impl(gbuffer)
{
    AssertThrow(gbuffer != nullptr);
}

GBuffer* ViewOutputTarget::GetGBuffer() const
{
    GBuffer* const* ptr = m_impl.TryGet<GBuffer*>();

    return ptr ? *ptr : nullptr;
}

const FramebufferRef& ViewOutputTarget::GetFramebuffer() const
{
    if (FramebufferRef const* framebuffer = m_impl.TryGet<FramebufferRef>())
    {
        AssertDebug(framebuffer->IsValid());

        return *framebuffer;
    }
    else if (GBuffer* const* gbuffer = m_impl.TryGet<GBuffer*>())
    {
        AssertDebug(*gbuffer != nullptr);

        return (*gbuffer)->GetBucket(BUCKET_OPAQUE).GetFramebuffer();
    }

    return FramebufferRef::Null();
}

const FramebufferRef& ViewOutputTarget::GetFramebuffer(Bucket bucket) const
{
    if (m_impl.Is<GBuffer*>())
    {
        return m_impl.Get<GBuffer*>()->GetBucket(bucket).GetFramebuffer();
    }

    return m_impl.Get<FramebufferRef>();
}

#pragma endregion ViewOutputTarget

#pragma region View

View::View()
    : View(ViewDesc {})
{
}

View::View(const ViewDesc& view_desc)
    : m_render_resource(nullptr),
      m_view_desc(view_desc),
      m_flags(view_desc.flags),
      m_viewport(view_desc.viewport),
      m_scenes(view_desc.scenes),
      m_camera(view_desc.camera),
      m_priority(view_desc.priority),
      m_override_attributes(view_desc.override_attributes)
{
}

View::~View()
{
    if (GBuffer* gbuffer = m_output_target.GetGBuffer())
    {
        delete gbuffer;

        m_output_target = ViewOutputTarget();
    }
    else if (FramebufferRef framebuffer = m_output_target.GetFramebuffer())
    {
        m_output_target = ViewOutputTarget();

        SafeRelease(std::move(framebuffer));
    }

    if (m_render_resource)
    {
        FreeResource(m_render_resource);
        m_render_resource = nullptr;
    }
}

void View::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
        {
            if (m_render_resource)
            {
                if (m_flags & ViewFlags::GBUFFER)
                {
                    m_render_resource->DecRef();
                }

                FreeResource(m_render_resource);
                m_render_resource = nullptr;
            }
        }));

    AssertThrowMsg(m_camera.IsValid(), "Camera is not valid for View with ID #%u", GetID().Value());
    InitObject(m_camera);

    if (m_view_desc.flags & ViewFlags::GBUFFER)
    {
        AssertDebugMsg(m_view_desc.output_target_desc.attachments.Empty(),
            "View with GBuffer flag cannot have output target attachments defined, as it will use GBuffer instead.");

        m_output_target = ViewOutputTarget(new GBuffer(Vec2u(m_viewport.extent)));
    }
    else if (m_view_desc.output_target_desc.attachments.Any())
    {
        FramebufferRef framebuffer = g_rendering_api->MakeFramebuffer(m_view_desc.output_target_desc.extent, m_view_desc.output_target_desc.num_views);

        for (uint32 attachment_index = 0; attachment_index < m_view_desc.output_target_desc.attachments.Size(); ++attachment_index)
        {
            const ViewOutputTargetAttachmentDesc& attachment_desc = m_view_desc.output_target_desc.attachments[attachment_index];

            AttachmentRef attachment = framebuffer->AddAttachment(
                attachment_index,
                attachment_desc.format,
                attachment_desc.image_type,
                attachment_desc.load_op,
                attachment_desc.store_op);

            attachment->SetClearColor(attachment_desc.clear_color);
        }

        DeferCreate(framebuffer);

        m_output_target = ViewOutputTarget(framebuffer);
    }

    Array<TResourceHandle<RenderScene>> render_scenes;
    render_scenes.Reserve(m_scenes.Size());

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrowMsg(scene.IsValid(), "Scene is not valid for View with ID #%u", GetID().Value());
        InitObject(scene);

        AssertThrow(scene->IsReady());

        render_scenes.PushBack(TResourceHandle<RenderScene>(scene->GetRenderResource()));
    }

    m_render_resource = AllocateResource<RenderView>(this);
    m_render_resource->SetViewport(m_viewport);

    SetReady(true);
}

bool View::TestRay(const Ray& ray, RayTestResults& out_results, bool use_bvh) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    bool has_hits = false;

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());

        if (scene->GetOctree().TestRay(ray, out_results, use_bvh))
        {
            has_hits = true;
        }
    }

    return has_hits;
}

void View::UpdateVisibility()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);
    AssertReady();

    if (!m_camera.IsValid())
    {
        HYP_LOG(Scene, Warning, "Camera is not valid for View with ID #%u, cannot update visibility!", GetID().Value());
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        scene->GetOctree().CalculateVisibility(m_camera);
    }
}

void View::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    // temp test
    RenderProxyList& rpl = GetProducerRenderProxyList(this);

    CollectLights();
    CollectLightmapVolumes();
    CollectEnvGrids();
    CollectEnvProbes();

    m_last_collection_result = CollectEntities(rpl);

    /// temp
    constexpr uint32 bucket_mask = (1 << BUCKET_OPAQUE)
        | (1 << BUCKET_LIGHTMAP)
        | (1 << BUCKET_SKYBOX)
        | (1 << BUCKET_TRANSLUCENT)
        | (1 << BUCKET_DEBUG);

    rpl.BuildRenderGroups(m_render_resource, m_override_attributes.TryGet());

    RenderCollector::CollectDrawCalls(rpl, bucket_mask);

    HYP_LOG(Scene, Debug, "Acquired proxy list : {} \t total count : {}", (void*)&rpl, rpl.NumRenderProxies());

    // constexpr uint32 bucket_mask = (1 << BUCKET_OPAQUE)
    //     | (1 << BUCKET_LIGHTMAP)
    //     | (1 << BUCKET_SKYBOX)
    //     | (1 << BUCKET_TRANSLUCENT)
    //     | (1 << BUCKET_DEBUG);

    // RenderCollector::CollectDrawCalls(m_render_resource->GetRenderProxyList(), bucket_mask);

    // HYP_LOG(Scene, Debug, "View #{} Update : Has {} scenes: {}, EnvGrids: {}, EnvProbes: {}, Lights: {}, Lightmap Volumes: {}",
    //     GetID().Value(),
    //     m_scenes.Size(),
    //     String::Join(Map(m_scenes, [](const Handle<Scene>& scene)
    //                      {
    //                          return HYP_FORMAT("(Name: {}, Nodes: {})", scene->GetName(), scene->GetRoot()->GetDescendants().Size());
    //                      }),
    //         ", "),
    //     m_tracked_env_grids.GetCurrentBits().Count(), m_tracked_env_probes.GetCurrentBits().Count(), m_tracked_lights.GetCurrentBits().Count(), m_tracked_lightmap_volumes.GetCurrentBits().Count());
}

void View::SetViewport(const Viewport& viewport)
{
    m_viewport = viewport;

    if (IsInitCalled())
    {
        m_render_resource->SetViewport(viewport);
    }
}

void View::SetPriority(int priority)
{
    m_priority = priority;

    if (IsInitCalled())
    {
        m_render_resource->SetPriority(priority);
    }
}

void View::AddScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;

    if (!scene.IsValid())
    {
        return;
    }

    if (m_scenes.Contains(scene))
    {
        return;
    }

    m_scenes.PushBack(scene);

    if (IsInitCalled())
    {
        InitObject(scene);
    }
}

void View::RemoveScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;

    if (!scene.IsValid())
    {
        return;
    }

    if (!m_scenes.Contains(scene))
    {
        return;
    }

    m_scenes.Erase(scene);
}

typename RenderProxyTracker::Diff View::CollectEntities(RenderProxyList& rpl)
{
    AssertReady();

    // switch (uint32(m_flags & ViewFlags::COLLECT_ALL_ENTITIES))
    // {
    // case uint32(ViewFlags::COLLECT_ALL_ENTITIES):
    return CollectAllEntities(rpl);
    // case uint32(ViewFlags::COLLECT_DYNAMIC_ENTITIES):
    //     return CollectDynamicEntities(rpl);
    // case uint32(ViewFlags::COLLECT_STATIC_ENTITIES):
    //     return CollectStaticEntities(rpl);
    // default:
    //     HYP_UNREACHABLE();
    // }
}

typename RenderProxyTracker::Diff View::CollectAllEntities(RenderProxyList& rpl)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

#if 0
    if (!m_camera.IsValid())
    {
        HYP_LOG(Scene, Warning, "Camera is not valid for View with ID #%u, cannot collect entities!", GetID().Value());

        auto diff = m_render_resource->UpdateTrackedRenderProxies(m_render_proxy_tracker);
        m_render_proxy_tracker.Advance(AdvanceAction::CLEAR);

        return diff;
    }

    const ID<Camera> camera_id = m_camera->GetID();

    const bool skip_frustum_culling = (m_flags & ViewFlags::SKIP_FRUSTUM_CULLING);

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());
        AssertThrow(scene->IsReady());

        if (scene->GetFlags() & SceneFlags::DETACHED)
        {
            HYP_LOG(Scene, Warning, "Scene \"{}\" has DETACHED flag set, cannot collect entities for render collector!", scene->GetName());

            continue;
        }

        const VisibilityStateSnapshot visibility_state_snapshot = scene->GetOctree().GetVisibilityState().GetSnapshot(camera_id);

        uint32 num_collected_entities = 0;
        uint32 num_skipped_entities = 0;

        for (auto [entity, mesh_component, visibility_state_component] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE))
            {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
                if (!visibility_state_component.visibility_state)
                {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                    ++num_skipped_entities;
#endif
                    continue;
                }

                if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot))
                {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                    ++num_skipped_entities;
#endif

                    continue;
                }
#endif
            }

            ++num_collected_entities;

            AssertDebug(mesh_component.proxy != nullptr);

            AssertDebug(mesh_component.proxy->entity.IsValid());
            AssertDebug(mesh_component.proxy->mesh.IsValid());
            AssertDebug(mesh_component.proxy->material.IsValid());

            m_render_proxy_tracker.Track(entity->GetID(), *mesh_component.proxy);
        }

#ifdef HYP_VISIBILITY_CHECK_DEBUG
        HYP_LOG(Scene, Debug, "Collected {} entities for camera {}, {} skipped", num_collected_entities, camera->GetName(), num_skipped_entities);
#endif
    }

    auto diff = m_render_resource->UpdateTrackedRenderProxies(m_render_proxy_tracker);
    m_render_proxy_tracker.Advance(AdvanceAction::CLEAR);
#else
    if (!m_camera.IsValid())
    {
        HYP_LOG(Scene, Warning, "Camera is not valid for View with ID #%u, cannot collect entities!", GetID().Value());

        // auto diff = rpl.SyncTrackedProxies(m_render_proxy_tracker, m_render_resource, m_override_attributes.TryGet());
        // m_render_proxy_tracker.Advance(AdvanceAction::CLEAR);

        return rpl.render_proxy_tracker.GetDiff();
    }

    const ID<Camera> camera_id = m_camera->GetID();

    const bool skip_frustum_culling = (m_flags & ViewFlags::SKIP_FRUSTUM_CULLING);

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());
        AssertThrow(scene->IsReady());

        if (scene->GetFlags() & SceneFlags::DETACHED)
        {
            HYP_LOG(Scene, Warning, "Scene \"{}\" has DETACHED flag set, cannot collect entities for render collector!", scene->GetName());

            continue;
        }

        const VisibilityStateSnapshot visibility_state_snapshot = scene->GetOctree().GetVisibilityState().GetSnapshot(camera_id);

        uint32 num_collected_entities = 0;
        uint32 num_skipped_entities = 0;

        for (auto [entity, mesh_component, visibility_state_component] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE))
            {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
                if (!visibility_state_component.visibility_state)
                {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                    ++num_skipped_entities;
#endif
                    continue;
                }

                if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot))
                {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                    ++num_skipped_entities;
#endif

                    continue;
                }
#endif
            }

            ++num_collected_entities;

            AssertDebug(mesh_component.proxy != nullptr);

            AssertDebug(mesh_component.proxy->entity.IsValid());
            AssertDebug(mesh_component.proxy->mesh.IsValid());
            AssertDebug(mesh_component.proxy->material.IsValid());

            rpl.render_proxy_tracker.Track(entity->GetID(), *mesh_component.proxy);
        }

#ifdef HYP_VISIBILITY_CHECK_DEBUG
        HYP_LOG(Scene, Debug, "Collected {} entities for View {}, {} skipped", num_collected_entities, GetID(), num_skipped_entities);
#endif
    }

    // auto diff = rpl.SyncTrackedProxies(m_render_proxy_tracker, m_render_resource, m_override_attributes.TryGet());
    return rpl.render_proxy_tracker.GetDiff();
#endif

    return {};
}

typename RenderProxyTracker::Diff View::CollectDynamicEntities(RenderProxyList& rpl)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertReady();

    if (!m_camera.IsValid())
    {
        HYP_LOG(Scene, Warning, "Camera is not valid for View with ID #%u, cannot collect dynamic entities!", GetID().Value());
        // if camera is invalid, update without adding any entities
        auto diff = m_render_resource->UpdateTrackedRenderProxies(m_render_proxy_tracker);
        m_render_proxy_tracker.Advance(AdvanceAction::CLEAR);

        return diff;
    }

    const ID<Camera> camera_id = m_camera->GetID();

    const bool skip_frustum_culling = (m_flags & ViewFlags::SKIP_FRUSTUM_CULLING);

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());
        AssertThrow(scene->IsReady());

        if (scene->GetFlags() & SceneFlags::DETACHED)
        {
            HYP_LOG(Scene, Warning, "Scene \"{}\" has DETACHED flag set, cannot collect entities for render collector!", scene->GetName());

            continue;
        }

        const VisibilityStateSnapshot visibility_state_snapshot = scene->GetOctree().GetVisibilityState().GetSnapshot(camera_id);

        for (auto [entity, mesh_component, visibility_state_component, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::DYNAMIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE))
            {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
                if (!visibility_state_component.visibility_state)
                {
                    continue;
                }

                if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot))
                {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                    HYP_LOG(Scene, Debug, "Skipping entity #{} for camera #{} due to visibility state being invalid.",
                        entity->GetID(), camera_id);
#endif

                    continue;
                }
#endif
            }

            AssertDebug(mesh_component.proxy != nullptr);

            AssertDebug(mesh_component.proxy->entity.IsValid());
            AssertDebug(mesh_component.proxy->mesh.IsValid());
            AssertDebug(mesh_component.proxy->material.IsValid());

            m_render_proxy_tracker.Track(entity->GetID(), *mesh_component.proxy);
        }
    }

    auto diff = m_render_resource->UpdateTrackedRenderProxies(m_render_proxy_tracker);
    m_render_proxy_tracker.Advance(AdvanceAction::CLEAR);

    return diff;
}

typename RenderProxyTracker::Diff View::CollectStaticEntities(RenderProxyList& rpl)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertReady();

    if (!m_camera.IsValid())
    {
        // if camera is invalid, update without adding any entities
        HYP_LOG(Scene, Warning, "Camera is not valid for View with ID #%u, cannot collect static entities!", GetID().Value());
        auto diff = m_render_resource->UpdateTrackedRenderProxies(m_render_proxy_tracker);
        m_render_proxy_tracker.Advance(AdvanceAction::CLEAR);

        return diff;
    }

    const ID<Camera> camera_id = m_camera->GetID();

    const bool skip_frustum_culling = (m_flags & ViewFlags::SKIP_FRUSTUM_CULLING);

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());
        AssertThrow(scene->IsReady());

        if (scene->GetFlags() & SceneFlags::DETACHED)
        {
            HYP_LOG(Scene, Warning, "Scene has DETACHED flag set, cannot collect entities for render collector!");

            return {};
        }

        const VisibilityStateSnapshot visibility_state_snapshot = scene->GetOctree().GetVisibilityState().GetSnapshot(camera_id);

        for (auto [entity, mesh_component, visibility_state_component, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::STATIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE))
            {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
                if (!visibility_state_component.visibility_state)
                {
                    continue;
                }

                if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot))
                {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                    HYP_LOG(Scene, Debug, "Skipping entity #{} for camera #{} due to visibility state being invalid.",
                        entity->GetID(), camera_id);
#endif

                    continue;
                }
#endif
            }

            AssertDebug(mesh_component.proxy != nullptr);

            AssertDebug(mesh_component.proxy->entity.IsValid());
            AssertDebug(mesh_component.proxy->mesh.IsValid());
            AssertDebug(mesh_component.proxy->material.IsValid());

            m_render_proxy_tracker.Track(entity->GetID(), *mesh_component.proxy);
        }
    }

    auto diff = m_render_resource->UpdateTrackedRenderProxies(m_render_proxy_tracker);
    m_render_proxy_tracker.Advance(AdvanceAction::CLEAR);

    return diff;
}

void View::CollectLights()
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_LIGHTS)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());
        AssertThrow(scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<Light>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            AssertDebug(entity->IsInstanceOf<Light>());

            Light* light = static_cast<Light*>(entity);

            bool is_light_in_frustum = false;

            switch (light->GetLightType())
            {
            case LightType::DIRECTIONAL:
                is_light_in_frustum = true;
                break;
            case LightType::POINT:
                is_light_in_frustum = m_camera->GetFrustum().ContainsBoundingSphere(light->GetBoundingSphere());
                break;
            case LightType::SPOT:
                // @TODO Implement frustum culling for spot lights
                is_light_in_frustum = true;
                break;
            case LightType::AREA_RECT:
                is_light_in_frustum = m_camera->GetFrustum().ContainsAABB(light->GetAABB());
                break;
            default:
                break;
            }

            if (is_light_in_frustum)
            {
                m_tracked_lights.Track(light->GetID(), &light->GetRenderResource());
            }

            /// TODO: Move this
            if (light->GetMutationState().IsDirty())
            {
                light->EnqueueRenderUpdates();
            }
        }
    }

    m_render_resource->UpdateTrackedLights(m_tracked_lights);
    m_tracked_lights.Advance(AdvanceAction::CLEAR);
}

void View::CollectLightmapVolumes()
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_LIGHTMAP_VOLUMES)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());
        AssertThrow(scene->IsReady());

        for (auto [entity, lightmap_volume_component] : scene->GetEntityManager()->GetEntitySet<LightmapVolumeComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!lightmap_volume_component.volume.IsValid())
            {
                continue;
            }

            const BoundingBox& volume_aabb = lightmap_volume_component.volume->GetAABB();

            if (!volume_aabb.IsValid() || !volume_aabb.IsFinite())
            {
                HYP_LOG(Scene, Warning, "Lightmap volume {} has an invalid AABB in view {}", lightmap_volume_component.volume->GetID(), GetID());

                continue;
            }

            if (!m_camera->GetFrustum().ContainsAABB(volume_aabb))
            {
                continue;
            }

            ResourceTracker<ID<LightmapVolume>, RenderLightmapVolume*>::ResourceTrackState track_state;

            m_tracked_lightmap_volumes.Track(
                lightmap_volume_component.volume->GetID(),
                &lightmap_volume_component.volume->GetRenderResource(),
                &track_state);

            if (track_state & ResourceTracker<ID<LightmapVolume>, RenderLightmapVolume*>::CHANGED)
            {
                HYP_LOG(Scene, Debug, "Lightmap volume {} changed in view {}", lightmap_volume_component.volume->GetID(), GetID());
            }
        }
    }

    m_render_resource->UpdateTrackedLightmapVolumes(m_tracked_lightmap_volumes);
    m_tracked_lightmap_volumes.Advance(AdvanceAction::CLEAR);
}

void View::CollectEnvGrids()
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_ENV_GRIDS)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());
        AssertThrow(scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvGrid>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            AssertDebug(entity->IsInstanceOf<EnvGrid>());

            EnvGrid* env_grid = static_cast<EnvGrid*>(entity);

            const BoundingBox& grid_aabb = env_grid->GetAABB();

            if (!grid_aabb.IsValid() || !grid_aabb.IsFinite())
            {
                HYP_LOG(Scene, Warning, "EnvGrid {} has an invalid AABB in view {}", env_grid->GetID(), GetID());

                continue;
            }

            if (!m_camera->GetFrustum().ContainsAABB(grid_aabb))
            {
                continue;
            }

            m_tracked_env_grids.Track(env_grid->GetID(), &env_grid->GetRenderResource());
        }
    }

    m_render_resource->UpdateTrackedEnvGrids(m_tracked_env_grids);
    m_tracked_env_grids.Advance(AdvanceAction::CLEAR);
}

void View::CollectEnvProbes()
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_ENV_PROBES)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());
        AssertThrow(scene->IsReady());

        for (auto [entity, reflection_probe_component] : scene->GetEntityManager()->GetEntitySet<ReflectionProbeComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!reflection_probe_component.env_probe.IsValid())
            {
                continue;
            }

            const BoundingBox& probe_aabb = reflection_probe_component.env_probe->GetAABB();

            if (!probe_aabb.IsValid() || !probe_aabb.IsFinite())
            {
                HYP_LOG(Scene, Warning, "EnvProbe {} has an invalid AABB in view {}", reflection_probe_component.env_probe->GetID(), GetID());

                continue;
            }

            if (!m_camera->GetFrustum().ContainsAABB(probe_aabb))
            {
                continue;
            }

            ResourceTracker<ID<EnvProbe>, RenderEnvProbe*>::ResourceTrackState track_state;
            m_tracked_env_probes.Track(
                reflection_probe_component.env_probe->GetID(),
                &reflection_probe_component.env_probe->GetRenderResource(),
                &track_state);
        }

        for (auto [entity, sky_component] : scene->GetEntityManager()->GetEntitySet<SkyComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!sky_component.subsystem.IsValid())
            {
                continue;
            }

            if (!sky_component.subsystem->GetEnvProbe().IsValid() || !sky_component.subsystem->GetEnvProbe()->IsReady())
            {
                HYP_LOG(Scene, Warning, "Sky component in scene {} does not have a valid EnvProbe in view {}", scene->GetID(), GetID());

                continue;
            }

            ResourceTracker<ID<EnvProbe>, RenderEnvProbe*>::ResourceTrackState track_state;
            m_tracked_env_probes.Track(
                sky_component.subsystem->GetEnvProbe()->GetID(),
                &sky_component.subsystem->GetEnvProbe()->GetRenderResource(),
                &track_state);
        }
    }

    /// TODO: point light Shadow maps

    m_render_resource->UpdateTrackedEnvProbes(m_tracked_env_probes);
    m_tracked_env_probes.Advance(AdvanceAction::CLEAR);
}

#pragma endregion View

} // namespace hyperion
