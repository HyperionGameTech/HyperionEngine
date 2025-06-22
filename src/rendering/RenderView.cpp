/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/SSGI.hpp>
#include <rendering/SSRRenderer.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/lightmapper/RenderLightmapVolume.hpp>
#include <rendering/debug/DebugDrawer.hpp>

#include <scene/View.hpp>
#include <scene/Texture.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>

#include <core/math/MathUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region RenderCollector helpers

static RenderableAttributeSet GetRenderableAttributesForProxy(const RenderProxy& proxy, const RenderableAttributeSet* override_attributes = nullptr)
{
    HYP_SCOPE;

    const Handle<Mesh>& mesh = proxy.mesh;
    AssertThrow(mesh.IsValid());

    const Handle<Material>& material = proxy.material;
    AssertThrow(material.IsValid());

    RenderableAttributeSet attributes {
        mesh->GetMeshAttributes(),
        material->GetRenderAttributes()
    };

    if (override_attributes)
    {
        if (const ShaderDefinition& override_shader_definition = override_attributes->GetShaderDefinition())
        {
            attributes.SetShaderDefinition(override_shader_definition);
        }

        const ShaderDefinition& shader_definition = override_attributes->GetShaderDefinition().IsValid()
            ? override_attributes->GetShaderDefinition()
            : attributes.GetShaderDefinition();

#ifdef HYP_DEBUG_MODE
        AssertThrow(shader_definition.IsValid());
#endif

        // Check for varying vertex attributes on the override shader compared to the entity's vertex
        // attributes. If there is not a match, we should switch to a version of the override shader that
        // has matching vertex attribs.
        const VertexAttributeSet mesh_vertex_attributes = attributes.GetMeshAttributes().vertex_attributes;

        MaterialAttributes new_material_attributes = override_attributes->GetMaterialAttributes();
        new_material_attributes.shader_definition = shader_definition;

        if (mesh_vertex_attributes != new_material_attributes.shader_definition.GetProperties().GetRequiredVertexAttributes())
        {
            new_material_attributes.shader_definition.properties.SetRequiredVertexAttributes(mesh_vertex_attributes);
        }

        // do not override bucket!
        new_material_attributes.bucket = attributes.GetMaterialAttributes().bucket;

        attributes.SetMaterialAttributes(new_material_attributes);
    }

    return attributes;
}

static void UpdateRenderableAttributesDynamic(const RenderProxy* proxy, RenderableAttributeSet& attributes)
{
    HYP_SCOPE;

    union
    {
        struct
        {
            bool has_instancing : 1;
            bool has_forward_lighting : 1;
        };

        uint64 overridden;
    };

    overridden = 0;

    has_instancing = proxy->instance_data.enable_auto_instancing || proxy->instance_data.num_instances > 1;
    has_forward_lighting = attributes.GetMaterialAttributes().bucket == BUCKET_TRANSLUCENT;

    if (!overridden)
    {
        return;
    }

    bool shader_definition_changed = false;
    ShaderDefinition shader_definition = attributes.GetShaderDefinition();

    if (has_instancing)
    {
        shader_definition.GetProperties().Set("INSTANCING");
        shader_definition_changed = true;
    }

    if (has_forward_lighting)
    {
        shader_definition.GetProperties().Set("FORWARD_LIGHTING");
        shader_definition_changed = true;
    }

    if (shader_definition_changed)
    {
        // Update the shader definition in the attributes
        attributes.SetShaderDefinition(shader_definition);
    }
}

HYP_DISABLE_OPTIMIZATION;
static void AddRenderProxy(RenderProxyList* render_proxy_list, RenderProxyTracker& render_proxy_tracker, RenderProxy&& proxy, RenderCamera* render_camera, RenderView* render_view, const RenderableAttributeSet& attributes, Bucket bucket)
{
    HYP_SCOPE;

    // Add proxy to group
    DrawCallCollectionMapping& mapping = render_proxy_list->mappings_by_bucket[int32(bucket)][attributes];
    Handle<RenderGroup>& rg = mapping.render_group;

    if (!rg.IsValid())
    {
        EnumFlags<RenderGroupFlags> render_group_flags = RenderGroupFlags::DEFAULT;

        // Disable occlusion culling for translucent objects
        const Bucket bucket = attributes.GetMaterialAttributes().bucket;

        if (bucket == BUCKET_TRANSLUCENT || bucket == BUCKET_DEBUG)
        {
            render_group_flags &= ~(RenderGroupFlags::OCCLUSION_CULLING | RenderGroupFlags::INDIRECT_RENDERING);
        }

        ShaderDefinition shader_definition = attributes.GetShaderDefinition();

        ShaderRef shader = g_shader_manager->GetOrCreate(shader_definition);

        if (!shader.IsValid())
        {
            HYP_LOG(Rendering, Error, "Failed to create shader for RenderProxy");

            return;
        }

        // Create RenderGroup
        rg = CreateObject<RenderGroup>(shader, attributes, render_group_flags);

        if (render_group_flags & RenderGroupFlags::INDIRECT_RENDERING)
        {
            AssertDebugMsg(mapping.indirect_renderer == nullptr, "Indirect renderer already exists on mapping");

            mapping.indirect_renderer = new IndirectRenderer();
            mapping.indirect_renderer->Create(rg->GetDrawCallCollectionImpl());

            mapping.draw_call_collection.impl = rg->GetDrawCallCollectionImpl();
        }

        AssertThrow(render_view->GetView() != nullptr);

        const ViewOutputTarget& output_target = render_view->GetView()->GetOutputTarget();

        const FramebufferRef& framebuffer = output_target.GetFramebuffer(attributes.GetMaterialAttributes().bucket);
        AssertThrow(framebuffer != nullptr);

        rg->AddFramebuffer(framebuffer);

        InitObject(rg);
    }

    const ID<Entity> entity_id = proxy.entity.GetID();
    auto iter = render_proxy_tracker.Track(entity_id, std::move(proxy));

    rg->AddRenderProxy(&iter->second);
}
HYP_ENABLE_OPTIMIZATION;

static bool RemoveRenderProxy(RenderProxyList* render_proxy_list, RenderProxyTracker& render_proxy_tracker, ID<Entity> entity, const RenderableAttributeSet& attributes, Bucket bucket)
{
    HYP_SCOPE;

    auto& mappings = render_proxy_list->mappings_by_bucket[uint32(bucket)];

    auto it = mappings.Find(attributes);
    AssertThrow(it != mappings.End());

    const DrawCallCollectionMapping& mapping = it->second;
    AssertThrow(mapping.IsValid());

    const bool removed = mapping.render_group->RemoveRenderProxy(entity);

    render_proxy_tracker.MarkToRemove(entity);

    return removed;
}

#pragma endregion RenderCollector helpers

#pragma region RenderView

RenderView::RenderView(View* view)
    : m_view(view),
      m_viewport(view ? view->GetViewport() : Viewport {}),
      m_priority(view ? view->GetPriority() : 0)
{
    m_lights.Resize(uint32(LightType::MAX));
    m_env_probes.Resize(uint32(EnvProbeType::MAX));
}

RenderView::~RenderView()
{
}

void RenderView::Initialize_Internal()
{
    HYP_SCOPE;

    if (m_view)
    {
        AssertThrow(m_view->GetCamera().IsValid());

        m_render_camera = TResourceHandle<RenderCamera>(m_view->GetCamera()->GetRenderResource());
    }

    // Reclaim any claimed resources that were unclaimed in Destroy_Internal
    RenderProxyTracker& render_proxy_tracker = m_render_proxy_list.render_proxy_tracker;

    Array<RenderProxy*> proxies;
    render_proxy_tracker.GetCurrent(proxies);

    for (RenderProxy* proxy : proxies)
    {
        proxy->IncRefs();
    }

    if (!m_view || (m_view->GetFlags() & ViewFlags::GBUFFER))
    {
        AssertThrow(m_viewport.extent.Volume() > 0);

        CreateRenderer();
    }
}

void RenderView::Destroy_Internal()
{
    HYP_SCOPE;

    // Unclaim all the claimed resources
    { // NOTE: We don't clear out the proxy list, we need to know which proxies to reclaim if this is reactivated
        RenderProxyTracker& render_proxy_tracker = m_render_proxy_list.render_proxy_tracker;

        Array<RenderProxy*> proxies;
        render_proxy_tracker.GetCurrent(proxies);

        for (RenderProxy* proxy : proxies)
        {
            proxy->DecRefs();
        }
    }

    { // lights
        Array<RenderLight*> lights;
        m_tracked_lights.GetCurrent(lights);

        for (RenderLight* light : lights)
        {
            light->DecRef();
        }

        m_tracked_lights.Reset();

        // Empty the lights by LightType arrays
        for (uint32 i = 0; i < uint32(LightType::MAX); i++)
        {
            m_lights[i].Clear();
        }
    }

    { // lightmap volumes
        Array<RenderLightmapVolume*> lightmap_volumes;
        m_tracked_lightmap_volumes.GetCurrent(lightmap_volumes);

        for (RenderLightmapVolume* lightmap_volume : lightmap_volumes)
        {
            lightmap_volume->DecRef();
        }

        m_tracked_lightmap_volumes.Reset();

        m_lightmap_volumes.Clear();
    }

    { // env grids
        Array<RenderEnvGrid*> env_grids;
        m_tracked_env_grids.GetCurrent(env_grids);

        for (RenderEnvGrid* env_grid : env_grids)
        {
            env_grid->DecRef();
        }

        m_tracked_env_grids.Reset();

        m_env_grids.Clear();
    }

    { // env probes
        Array<RenderEnvProbe*> env_probes;
        m_tracked_env_probes.GetCurrent(env_probes);

        for (RenderEnvProbe* env_probe : env_probes)
        {
            env_probe->DecRef();
        }

        m_tracked_env_probes.Reset();

        for (uint32 i = 0; i < uint32(EnvProbeType::MAX); i++)
        {
            m_env_probes[i].Clear();
        }
    }

    if (m_view->GetFlags() & ViewFlags::GBUFFER)
    {
        DestroyRenderer();
    }

    m_render_camera.Reset();
}

GBuffer* RenderView::GetGBuffer() const
{
    if (!m_view)
    {
        return nullptr;
    }

    return m_view->GetOutputTarget().GetGBuffer();
}

void RenderView::Update_Internal()
{
    HYP_SCOPE;
}

void RenderView::CreateRenderer()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);
}

void RenderView::DestroyRenderer()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);
}

void RenderView::SetViewport(const Viewport& viewport)
{
    HYP_SCOPE;

    Execute([this, viewport]()
        {
            if (m_viewport == viewport)
            {
                return;
            }

            m_viewport = viewport;

            if (IsInitialized() && (m_view->GetFlags() & ViewFlags::GBUFFER))
            {
            }
        });
}

void RenderView::SetPriority(int priority)
{
    HYP_SCOPE;

    Execute([this, priority]()
        {
            m_priority = priority;
        });
}

typename RenderProxyTracker::Diff RenderView::UpdateTrackedRenderProxies(const RenderProxyTracker& render_proxy_tracker)
{
    HYP_SCOPE;
    Threads::AssertOnThread(~g_render_thread);

    typename RenderProxyTracker::Diff diff = render_proxy_tracker.GetDiff();

    if (diff.NeedsUpdate())
    {
        Array<ID<Entity>> removed_proxies;
        render_proxy_tracker.GetRemoved(removed_proxies, true /* include_changed */);

        Array<RenderProxy*> added_proxy_ptrs;
        render_proxy_tracker.GetAdded(added_proxy_ptrs, true /* include_changed */);

        if (added_proxy_ptrs.Any() || removed_proxies.Any())
        {
            Array<RenderProxy, DynamicAllocator> added_proxies;
            added_proxies.Reserve(added_proxy_ptrs.Size());

            for (RenderProxy* proxy_ptr : added_proxy_ptrs)
            {
                added_proxies.PushBack(*proxy_ptr);
            }

            Execute([this, added_proxies = std::move(added_proxies), removed_proxies = std::move(removed_proxies)]() mutable
                {
                    AssertDebugMsg(IsInitialized(), "UpdateTrackedRenderProxies can only be called on an initialized RenderView");

                    const RenderableAttributeSet* override_attributes = m_view ? m_view->GetOverrideAttributes().TryGet() : nullptr;

                    RenderProxyTracker& render_proxy_tracker = m_render_proxy_list.render_proxy_tracker;

                    // Reserve to prevent iterator invalidation (pointers to proxies are stored in the render groups)
                    render_proxy_tracker.Reserve(added_proxies.Size());

                    // Claim the added(+changed) before unclaiming the removed, as we may end up doing extra work for changed entities otherwise (unclaiming then reclaiming)
                    for (RenderProxy& proxy : added_proxies)
                    {
                        proxy.IncRefs();
                    }

                    for (ID<Entity> entity_id : removed_proxies)
                    {
                        const RenderProxy* proxy = render_proxy_tracker.GetElement(entity_id);
                        AssertThrowMsg(proxy != nullptr, "Proxy is missing for Entity #%u", entity_id.Value());

                        proxy->DecRefs();

                        RenderableAttributeSet attributes = GetRenderableAttributesForProxy(*proxy, override_attributes);
                        UpdateRenderableAttributesDynamic(proxy, attributes);

                        const Bucket bucket = attributes.GetMaterialAttributes().bucket;

                        AssertThrow(RemoveRenderProxy(&m_render_proxy_list, render_proxy_tracker, entity_id, attributes, bucket));
                    }

                    for (RenderProxy& proxy : added_proxies)
                    {
                        const Handle<Mesh>& mesh = proxy.mesh;
                        AssertThrow(mesh.IsValid());
                        AssertThrow(mesh->IsReady());

                        const Handle<Material>& material = proxy.material;
                        AssertThrow(material.IsValid());

                        RenderableAttributeSet attributes = GetRenderableAttributesForProxy(proxy, override_attributes);
                        UpdateRenderableAttributesDynamic(&proxy, attributes);

                        const Bucket bucket = attributes.GetMaterialAttributes().bucket;

                        AddRenderProxy(&m_render_proxy_list, render_proxy_tracker, std::move(proxy), m_render_camera.Get(), this, attributes, bucket);
                    }

                    render_proxy_tracker.Advance(AdvanceAction::PERSIST);

                    // Clear out groups that are no longer used
                    m_render_proxy_list.RemoveEmptyProxyGroups();
                });
        }
    }

    return diff;
}

void RenderView::UpdateTrackedLights(const ResourceTracker<ID<Light>, RenderLight*>& tracked_lights)
{
    if (!tracked_lights.GetDiff().NeedsUpdate())
    {
        return;
    }

    Array<ID<Light>, DynamicAllocator> removed;
    tracked_lights.GetRemoved(removed, true /* include_changed */);

    Array<RenderLight*, DynamicAllocator> added;
    tracked_lights.GetAdded(added, true /* include_changed */);

    for (RenderLight* render_light : added)
    {
        // Ensure it doesn't get deleted while we queue the update txn
        render_light->IncRef();
    }

    Execute([this, added = std::move(added), removed = std::move(removed)]() mutable
        {
            if (!m_tracked_lights.WasReset())
            {
                for (ID<Light> id : removed)
                {
                    RenderLight** element_ptr = m_tracked_lights.GetElement(id);
                    AssertDebug(element_ptr != nullptr);

                    RenderLight* render_light = *element_ptr;
                    AssertDebug(render_light);

                    const LightType light_type = render_light->GetLight()->GetLightType();

                    auto it = m_lights[uint32(light_type)].Find(render_light);
                    AssertDebug(it != m_lights[uint32(light_type)].End());

                    m_lights[uint32(light_type)].Erase(it);

                    render_light->DecRef();

                    m_tracked_lights.MarkToRemove(id);
                }
            }

            for (RenderLight* render_light : added)
            {
                const LightType light_type = render_light->GetLight()->GetLightType();
                AssertDebug(!m_lights[uint32(light_type)].Contains(render_light));

                m_lights[uint32(light_type)].PushBack(render_light);

                m_tracked_lights.Track(render_light->GetLight()->GetID(), render_light);
            }

            m_tracked_lights.Advance(AdvanceAction::PERSIST);
        });
}

void RenderView::UpdateTrackedLightmapVolumes(const ResourceTracker<ID<LightmapVolume>, RenderLightmapVolume*>& tracked_lightmap_volumes)
{
    if (!tracked_lightmap_volumes.GetDiff().NeedsUpdate())
    {
        return;
    }

    Array<ID<LightmapVolume>, DynamicAllocator> removed;
    tracked_lightmap_volumes.GetRemoved(removed, true /* include_changed */);

    Array<RenderLightmapVolume*, DynamicAllocator> added;
    tracked_lightmap_volumes.GetAdded(added, true /* include_changed */);

    for (RenderLightmapVolume* render_lightmap_volume : added)
    {
        // Ensure it doesn't get deleted while we queue the update txn
        render_lightmap_volume->IncRef();
    }

    Execute([this, added = std::move(added), removed = std::move(removed)]() mutable
        {
            if (!m_tracked_lightmap_volumes.WasReset())
            {
                for (ID<LightmapVolume> id : removed)
                {
                    RenderLightmapVolume** element_ptr = m_tracked_lightmap_volumes.GetElement(id);
                    AssertDebug(element_ptr != nullptr);

                    RenderLightmapVolume* render_lightmap_volume = *element_ptr;
                    AssertDebug(render_lightmap_volume);

                    auto it = m_lightmap_volumes.Find(render_lightmap_volume);
                    AssertDebug(it != m_lightmap_volumes.End());

                    m_lightmap_volumes.Erase(it);

                    m_tracked_lightmap_volumes.MarkToRemove(id);

                    render_lightmap_volume->DecRef();
                }
            }

            for (RenderLightmapVolume* render_lightmap_volume : added)
            {
                AssertDebug(!m_lightmap_volumes.Contains(render_lightmap_volume));

                m_lightmap_volumes.PushBack(render_lightmap_volume);

                m_tracked_lightmap_volumes.Track(render_lightmap_volume->GetLightmapVolume()->GetID(), render_lightmap_volume);
            }

            m_tracked_lightmap_volumes.Advance(AdvanceAction::PERSIST);
        });
}

void RenderView::UpdateTrackedEnvGrids(const ResourceTracker<ID<EnvGrid>, RenderEnvGrid*>& tracked_env_grids)
{
    if (!tracked_env_grids.GetDiff().NeedsUpdate())
    {
        return;
    }

    Array<ID<EnvGrid>, DynamicAllocator> removed;
    tracked_env_grids.GetRemoved(removed, true /* include_changed */);

    Array<RenderEnvGrid*, DynamicAllocator> added;
    tracked_env_grids.GetAdded(added, true /* include_changed */);

    for (RenderEnvGrid* render_env_grid : added)
    {
        // Ensure it doesn't get deleted while we queue the update txn
        render_env_grid->IncRef();
    }

    Execute([this, added = std::move(added), removed = std::move(removed)]() mutable
        {
            if (!m_tracked_env_grids.WasReset())
            {
                for (ID<EnvGrid> id : removed)
                {
                    RenderEnvGrid** element_ptr = m_tracked_env_grids.GetElement(id);
                    AssertDebug(element_ptr != nullptr);

                    RenderEnvGrid* render_env_grid = *element_ptr;
                    AssertDebug(render_env_grid);

                    auto it = m_env_grids.Find(render_env_grid);
                    AssertDebug(it != m_env_grids.End());

                    m_env_grids.Erase(it);

                    m_tracked_env_grids.MarkToRemove(id);

                    render_env_grid->DecRef();
                }
            }

            for (RenderEnvGrid* render_env_grid : added)
            {
                AssertDebug(!m_env_grids.Contains(render_env_grid));

                m_env_grids.PushBack(render_env_grid);

                m_tracked_env_grids.Track(render_env_grid->GetEnvGrid()->GetID(), render_env_grid);
            }

            m_tracked_env_grids.Advance(AdvanceAction::PERSIST);
        });
}

void RenderView::UpdateTrackedEnvProbes(const ResourceTracker<ID<EnvProbe>, RenderEnvProbe*>& tracked_env_probes)
{
    if (!tracked_env_probes.GetDiff().NeedsUpdate())
    {
        return;
    }

    Array<ID<EnvProbe>, DynamicAllocator> removed;
    tracked_env_probes.GetRemoved(removed, true /* include_changed */);

    Array<RenderEnvProbe*, DynamicAllocator> added;
    tracked_env_probes.GetAdded(added, true /* include_changed */);

    for (RenderEnvProbe* render_env_probe : added)
    {
        // Ensure it doesn't get deleted while we queue the update txn
        render_env_probe->IncRef();
    }

    Execute([this, added = std::move(added), removed = std::move(removed)]() mutable
        {
            if (!m_tracked_lights.WasReset())
            {
                for (ID<EnvProbe> id : removed)
                {
                    RenderEnvProbe** element_ptr = m_tracked_env_probes.GetElement(id);
                    AssertDebug(element_ptr != nullptr);

                    RenderEnvProbe* render_env_probe = *element_ptr;
                    AssertDebug(render_env_probe);

                    const EnvProbeType env_probe_type = render_env_probe->GetEnvProbe()->GetEnvProbeType();

                    auto it = m_env_probes[uint32(env_probe_type)].Find(render_env_probe);
                    AssertDebug(it != m_env_probes[uint32(env_probe_type)].End());

                    m_env_probes[uint32(env_probe_type)].Erase(it);

                    render_env_probe->DecRef();

                    m_tracked_env_probes.MarkToRemove(id);
                }
            }

            for (RenderEnvProbe* render_env_probe : added)
            {
                const EnvProbeType env_probe_type = render_env_probe->GetEnvProbe()->GetEnvProbeType();
                AssertDebug(!m_env_probes[uint32(env_probe_type)].Contains(render_env_probe));

                m_env_probes[uint32(env_probe_type)].PushBack(render_env_probe);

                m_tracked_env_probes.Track(render_env_probe->GetEnvProbe()->GetID(), render_env_probe);
            }

            m_tracked_env_probes.Advance(AdvanceAction::PERSIST);
        });
}

void RenderView::PreRender(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(IsInitialized());

    // if (m_post_processing)
    // {
    //     m_post_processing->PerformUpdates();
    // }
}

void RenderView::PostRender(FrameBase* frame)
{
}

#pragma endregion RenderView

} // namespace hyperion
