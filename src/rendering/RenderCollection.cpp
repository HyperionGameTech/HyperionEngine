/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderCollection.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderView.hpp>

#include <rendering/backend/RenderingAPI.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <scene/Scene.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/View.hpp>

#include <scene/camera/Camera.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/Util.hpp>

#include <Engine.hpp>

namespace hyperion {

static constexpr bool do_parallel_collection = false; // true;

#pragma region Render commands

struct RENDER_COMMAND(RebuildProxyGroups)
    : renderer::RenderCommand
{
    TResourceHandle<RenderView> render_view;

    RC<RenderProxyList> render_proxy_list;
    Array<RenderProxy> added_proxies;
    Array<ID<Entity>> removed_proxies;

    Handle<Camera> camera;
    Optional<RenderableAttributeSet> override_attributes;

    RENDER_COMMAND(RebuildProxyGroups)(
        const TResourceHandle<RenderView>& render_view,
        const RC<RenderProxyList>& render_proxy_list,
        Array<RenderProxy*>&& added_proxy_ptrs,
        Array<ID<Entity>>&& removed_proxies,
        const Handle<Camera>& camera = Handle<Camera>::empty,
        const Optional<RenderableAttributeSet>& override_attributes = {})
        : render_view(render_view),
          render_proxy_list(render_proxy_list),
          removed_proxies(std::move(removed_proxies)),
          camera(camera),
          override_attributes(override_attributes)
    {
        AssertThrow(render_view);

        added_proxies.Reserve(added_proxy_ptrs.Size());

        for (RenderProxy* proxy_ptr : added_proxy_ptrs)
        {
            added_proxies.PushBack(*proxy_ptr);
        }
    }

    virtual ~RENDER_COMMAND(RebuildProxyGroups)() override = default;

    RenderableAttributeSet GetRenderableAttributesForProxy(const RenderProxy& proxy) const
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

        if (override_attributes.HasValue())
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

    void AddRenderProxy(RenderProxyTracker& render_proxy_tracker, RenderProxy&& proxy, const RenderableAttributeSet& attributes, Bucket bucket)
    {
        HYP_SCOPE;

        const ID<Entity> entity = proxy.entity.GetID();

        // Add proxy to group
        DrawCallCollectionMapping& mapping = render_proxy_list->mappings_by_bucket[uint32(bucket)][attributes];
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

            // Create RenderGroup
            rg = CreateObject<RenderGroup>(
                g_shader_manager->GetOrCreate(attributes.GetShaderDefinition()),
                attributes,
                render_group_flags);

            if (render_group_flags & RenderGroupFlags::INDIRECT_RENDERING)
            {
                AssertDebugMsg(mapping.indirect_renderer == nullptr, "Indirect renderer already exists on mapping");

                mapping.indirect_renderer = new IndirectRenderer();
                mapping.indirect_renderer->Create(rg->GetDrawCallCollectionImpl());

                mapping.draw_call_collection.impl = rg->GetDrawCallCollectionImpl();
            }

            FramebufferRef framebuffer;

            if (camera.IsValid())
            {
                framebuffer = camera->GetRenderResource().GetFramebuffer();
            }

            if (framebuffer != nullptr)
            {
                rg->AddFramebuffer(framebuffer);
            }
            else
            {
                AssertThrow(render_view->GetView()->GetFlags() & ViewFlags::GBUFFER);

                GBuffer* gbuffer = render_view->GetGBuffer();
                AssertThrow(gbuffer != nullptr);

                const FramebufferRef& bucket_framebuffer = gbuffer->GetBucket(attributes.GetMaterialAttributes().bucket).GetFramebuffer();
                AssertThrow(bucket_framebuffer != nullptr);

                rg->AddFramebuffer(bucket_framebuffer);
            }

            InitObject(rg);
        }

        auto iter = render_proxy_tracker.Track(entity, std::move(proxy));

        rg->AddRenderProxy(&iter->second);
    }

    bool RemoveRenderProxy(RenderProxyTracker& render_proxy_tracker, ID<Entity> entity, const RenderableAttributeSet& attributes, Bucket bucket)
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

    virtual RendererResult operator()() override
    {
        HYP_SCOPE;

        RenderProxyTracker& render_proxy_tracker = render_proxy_list->render_proxy_tracker;

        // Reserve to prevent iterator invalidation (pointers to proxies are stored in the render groups)
        render_proxy_tracker.Reserve(added_proxies.Size());

#if 0
        HYP_LOG(RenderCollection, Debug,
            "Added: {} | Removed: {}",
            added_proxies.Size(),
            removed_proxies.Size()
        );

        for (const auto &added : added_proxies) {
            HYP_LOG(
                RenderCollection,
                Debug,
                "Added proxy for entity {} (mesh={}, count: {}, material={}, count: {})",
                added.entity->GetID(),
                added.mesh ? *added.mesh->GetName() : "null", added.mesh ? added.mesh->GetRenderResource().NumRefs() : 0,
                added.material ? *added.material->GetName() : "null", added.material ? added.material->GetRenderResource().NumRefs() : 0
            );
        }

        for (const auto &removed : removed_proxies) {
            HYP_LOG(
                RenderCollection,
                Debug,
                "Removed proxy for entity {}",
                removed
            );
        }
#endif
        for (RenderProxy& proxy : added_proxies)
        {
            proxy.IncRefs();
        }

        for (ID<Entity> entity_id : removed_proxies)
        {
            const RenderProxy* proxy = render_proxy_tracker.GetElement(entity_id);
            AssertThrowMsg(proxy != nullptr, "Proxy is missing for Entity #%u", entity_id.Value());

            proxy->DecRefs();

            const RenderableAttributeSet attributes = GetRenderableAttributesForProxy(*proxy);
            const Bucket bucket = attributes.GetMaterialAttributes().bucket;

            AssertThrow(RemoveRenderProxy(render_proxy_tracker, entity_id, attributes, bucket));
        }

        for (RenderProxy& proxy : added_proxies)
        {
            const Handle<Mesh>& mesh = proxy.mesh;
            AssertThrow(mesh.IsValid());
            AssertThrow(mesh->IsReady());

            const Handle<Material>& material = proxy.material;
            AssertThrow(material.IsValid());

            const RenderableAttributeSet attributes = GetRenderableAttributesForProxy(proxy);
            const Bucket bucket = attributes.GetMaterialAttributes().bucket;

            AddRenderProxy(render_proxy_tracker, std::move(proxy), attributes, bucket);
        }

        render_proxy_tracker.Advance(AdvanceAction::PERSIST);

        // Clear out groups that are no longer used
        render_proxy_list->RemoveEmptyProxyGroups();

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region RenderProxyList

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

static void AddRenderProxy(RenderProxyList* render_proxy_list, RenderProxyTracker& render_proxy_tracker, RenderProxy* proxy, RenderView* render_view, const RenderableAttributeSet& attributes, Bucket bucket)
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

    rg->AddRenderProxy(proxy);
}

static bool RemoveRenderProxy(RenderProxyList* render_proxy_list, RenderProxyTracker& render_proxy_tracker, ID<Entity> entity, const RenderableAttributeSet& attributes, Bucket bucket)
{
    HYP_SCOPE;

    auto& mappings = render_proxy_list->mappings_by_bucket[uint32(bucket)];

    auto it = mappings.Find(attributes);
    AssertThrow(it != mappings.End());

    const DrawCallCollectionMapping& mapping = it->second;
    AssertThrow(mapping.IsValid());

    const bool removed = mapping.render_group->RemoveRenderProxy(entity);

    return removed;
}

RenderProxyList::RenderProxyList()
    : parallel_rendering_state_head(nullptr),
      parallel_rendering_state_tail(nullptr)
{
}

RenderProxyList::~RenderProxyList()
{
    if (parallel_rendering_state_head)
    {
        ParallelRenderingState* state = parallel_rendering_state_head;

        while (state != nullptr)
        {
            if (state->task_batch != nullptr)
            {
                state->task_batch->AwaitCompletion();
                delete state->task_batch;
            }

            ParallelRenderingState* next_state = state->next;

            delete state;

            state = next_state;
        }
    }

    ClearProxyGroups();
}

void RenderProxyList::ClearProxyGroups()
{
    HYP_SCOPE;

    // Do not fully clear, keep the attribs around so that we can have memory reserved for each slot,
    // as well as render groups.
    for (auto& mappings : mappings_by_bucket)
    {
        for (auto& it : mappings)
        {
            DrawCallCollectionMapping& mapping = it.second;

            if (mapping.render_group.IsValid())
            {
                mapping.render_group->ClearProxies();
            }

            if (mapping.indirect_renderer)
            {
                delete mapping.indirect_renderer;
                mapping.indirect_renderer = nullptr;
            }
        }
    }
}

void RenderProxyList::RemoveEmptyProxyGroups()
{
    HYP_SCOPE;

    for (auto& mappings : mappings_by_bucket)
    {
        for (auto it = mappings.Begin(); it != mappings.End();)
        {
            DrawCallCollectionMapping& mapping = it->second;
            AssertDebug(mapping.IsValid());

            if (mapping.render_group->GetRenderProxies().Any())
            {
                ++it;

                continue;
            }

            if (mapping.indirect_renderer)
            {
                delete mapping.indirect_renderer;
                mapping.indirect_renderer = nullptr;
            }

            it = mappings.Erase(it);
        }
    }
}

uint32 RenderProxyList::NumRenderGroups() const
{
    uint32 count = 0;

    for (const auto& mappings : mappings_by_bucket)
    {
        for (const auto& it : mappings)
        {
            const DrawCallCollectionMapping& mapping = it.second;
            AssertDebug(mapping.IsValid());

            if (mapping.IsValid())
            {
                ++count;
            }
        }
    }

    return count;
}

uint32 RenderProxyList::NumRenderProxies() const
{
    uint32 count = 0;

    for (const auto& mappings : mappings_by_bucket)
    {
        for (const auto& it : mappings)
        {
            const DrawCallCollectionMapping& mapping = it.second;
            AssertDebug(mapping.IsValid());

            if (mapping.IsValid())
            {
                count += mapping.render_group->GetRenderProxies().Size();
            }
        }
    }

    return count;
}

void RenderProxyList::BuildRenderGroups(RenderView* render_view, const RenderableAttributeSet* override_attributes)
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
            // Claim the added(+changed) before unclaiming the removed, as we may end up doing extra work for changed entities otherwise (unclaiming then reclaiming)
            for (RenderProxy* proxy : added_proxy_ptrs)
            {
                AssertDebug(proxy != nullptr);

                proxy->IncRefs();
            }

            for (ID<Entity> entity_id : removed_proxies)
            {
                const RenderProxy* proxy = render_proxy_tracker.GetElement(entity_id);
                AssertThrowMsg(proxy != nullptr, "Proxy is missing for Entity #%u", entity_id.Value());

                proxy->DecRefs();

                RenderableAttributeSet attributes = GetRenderableAttributesForProxy(*proxy, override_attributes);
                UpdateRenderableAttributesDynamic(proxy, attributes);

                const Bucket bucket = attributes.GetMaterialAttributes().bucket;

                AssertThrow(RemoveRenderProxy(this, render_proxy_tracker, entity_id, attributes, bucket));
            }

            for (RenderProxy* proxy : added_proxy_ptrs)
            {
                const Handle<Mesh>& mesh = proxy->mesh;
                AssertThrow(mesh.IsValid());
                AssertThrow(mesh->IsReady());

                const Handle<Material>& material = proxy->material;
                AssertThrow(material.IsValid());

                RenderableAttributeSet attributes = GetRenderableAttributesForProxy(*proxy, override_attributes);
                UpdateRenderableAttributesDynamic(proxy, attributes);

                const Bucket bucket = attributes.GetMaterialAttributes().bucket;

                AddRenderProxy(this, render_proxy_tracker, proxy, render_view, attributes, bucket);
            }

            render_proxy_tracker.Advance(AdvanceAction::PERSIST);

            // Clear out groups that are no longer used
            RemoveEmptyProxyGroups();
        }
    }
}

ParallelRenderingState* RenderProxyList::AcquireNextParallelRenderingState()
{
    ParallelRenderingState* curr = parallel_rendering_state_tail;

    if (!curr)
    {
        if (!parallel_rendering_state_head)
        {
            parallel_rendering_state_head = new ParallelRenderingState();

            TaskThreadPool& pool = TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER);

            TaskBatch* task_batch = new TaskBatch();
            task_batch->pool = &pool;

            parallel_rendering_state_head->task_batch = task_batch;
            parallel_rendering_state_head->num_batches = ParallelRenderingState::max_batches;
        }

        curr = parallel_rendering_state_head;
    }
    else
    {
        if (!curr->next)
        {
            ParallelRenderingState* new_parallel_rendering_state = new ParallelRenderingState();

            TaskThreadPool& pool = TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER);

            TaskBatch* task_batch = new TaskBatch();
            task_batch->pool = &pool;

            new_parallel_rendering_state->task_batch = task_batch;
            new_parallel_rendering_state->num_batches = ParallelRenderingState::max_batches;

            curr->next = new_parallel_rendering_state;
        }

        curr = curr->next;
    }

    parallel_rendering_state_tail = curr;

    AssertDebug(curr != nullptr);
    AssertDebug(curr->task_batch != nullptr);
    AssertDebug(curr->task_batch->IsCompleted());

    return curr;
}

void RenderProxyList::CommitParallelRenderingState(RHICommandList& out_command_list)
{
    ParallelRenderingState* state = parallel_rendering_state_head;

    while (state)
    {
        AssertDebug(state->task_batch != nullptr);

        state->task_batch->AwaitCompletion();

        out_command_list.Concat(std::move(state->base_command_list));

        // Add command lists to the frame's command list
        for (RHICommandList& command_list : state->command_lists)
        {
            out_command_list.Concat(std::move(command_list));
        }

        // Add render stats counts to the engine's render stats
        for (EngineRenderStatsCounts& counts : state->render_stats_counts)
        {
            g_engine->GetRenderStatsCalculator().AddCounts(counts);

            counts = EngineRenderStatsCounts(); // Reset counts after adding for next use
        }

        state->draw_calls.Clear();
        state->draw_call_procs.Clear();
        state->instanced_draw_calls.Clear();
        state->instanced_draw_call_procs.Clear();

        state->task_batch->ResetState();

        state = state->next;
    }

    parallel_rendering_state_tail = nullptr;
}

#pragma endregion RenderProxyList

#pragma region RenderCollector

void RenderCollector::CollectDrawCalls(RenderProxyList& render_proxy_list, uint32 bucket_bits)
{
    HYP_SCOPE;

    if (!ByteUtil::BitCount(bucket_bits))
    {
        return;
    }

    HYP_MT_CHECK_RW(render_proxy_list.data_race_detector);

    using IteratorType = FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>::Iterator;
    Array<IteratorType> iterators;

    FOR_EACH_BIT(bucket_bits, bit_index)
    {
        AssertDebug(bit_index < render_proxy_list.mappings_by_bucket.Size());

        auto& mappings = render_proxy_list.mappings_by_bucket[bit_index];

        if (mappings.Empty())
        {
            continue;
        }

        for (auto& it : mappings)
        {
            iterators.PushBack(&it);
        }
    }

    if (iterators.Empty())
    {
        return;
    }

    for (IteratorType it : iterators)
    {
        const RenderableAttributeSet& attributes = it->first;

        DrawCallCollectionMapping& mapping = it->second;
        AssertDebug(mapping.IsValid());

        mapping.render_group->CollectDrawCalls(mapping.draw_call_collection);
    }
}

void RenderCollector::PerformOcclusionCulling(FrameBase* frame, const RenderSetup& render_setup, RenderProxyList& render_proxy_list, uint32 bucket_bits)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertDebug(render_setup.IsValid());
    AssertDebugMsg(render_setup.HasView(), "RenderSetup must have a View attached");
    AssertDebugMsg(render_setup.pass_data != nullptr, "RenderSetup must have valid PassData to perform occlusion culling");

    HYP_MT_CHECK_RW(render_proxy_list.data_race_detector);

    static const bool is_indirect_rendering_enabled = g_rendering_api->GetRenderConfig().IsIndirectRenderingEnabled();
    const bool perform_occlusion_culling = is_indirect_rendering_enabled && render_setup.pass_data->cull_data.depth_pyramid_image_view != nullptr;

    if (perform_occlusion_culling)
    {
        FOR_EACH_BIT(bucket_bits, bit_index)
        {
            AssertDebug(bit_index < render_proxy_list.mappings_by_bucket.Size());

            auto& mappings = render_proxy_list.mappings_by_bucket[bit_index];

            if (mappings.Empty())
            {
                continue;
            }

            for (auto& it : mappings)
            {
                DrawCallCollectionMapping& mapping = it.second;
                AssertDebug(mapping.IsValid());

                const Handle<RenderGroup>& render_group = mapping.render_group;
                AssertDebug(render_group.IsValid());

                DrawCallCollection& draw_call_collection = mapping.draw_call_collection;
                IndirectRenderer* indirect_renderer = mapping.indirect_renderer;

                if (render_group->GetFlags() & RenderGroupFlags::OCCLUSION_CULLING)
                {
                    AssertDebug((render_group->GetFlags() & (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING)) == (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING));
                    AssertDebug(indirect_renderer != nullptr);

                    indirect_renderer->GetDrawState().ResetDrawState();

                    indirect_renderer->PushDrawCallsToIndirectState(draw_call_collection);
                    indirect_renderer->ExecuteCullShaderInBatches(frame, render_setup);
                }
            }
        }
    }
}

void RenderCollector::ExecuteDrawCalls(
    FrameBase* frame,
    const RenderSetup& render_setup,
    RenderProxyList& render_proxy_list,
    uint32 bucket_bits,
    PushConstantData push_constant)
{
    AssertDebug(render_setup.IsValid());
    AssertDebugMsg(render_setup.HasView(), "RenderSetup must have a View attached");

    if (render_setup.view->GetView()->GetFlags() & ViewFlags::GBUFFER)
    {
        // Pass NULL framebuffer for GBuffer rendering, as it will be handled by the GBuffer renderer outside of this.
        ExecuteDrawCalls(frame, render_setup, render_proxy_list, FramebufferRef::Null(), bucket_bits, push_constant);
    }
    else
    {
        const FramebufferRef& framebuffer = render_setup.view->GetView()->GetOutputTarget().GetFramebuffer();
        AssertDebugMsg(framebuffer != nullptr, "Must have a valid framebuffer for rendering");

        ExecuteDrawCalls(frame, render_setup, render_proxy_list, framebuffer, bucket_bits, push_constant);
    }
}

void RenderCollector::ExecuteDrawCalls(
    FrameBase* frame,
    const RenderSetup& render_setup,
    RenderProxyList& render_proxy_list,
    const FramebufferRef& framebuffer,
    uint32 bucket_bits,
    PushConstantData push_constant)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    if (!ByteUtil::BitCount(bucket_bits))
    {
        return;
    }

    HYP_MT_CHECK_RW(render_proxy_list.data_race_detector);

    Span<FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>> groups_view;

    // If only one bit is set, we can skip the loop by directly accessing the RenderGroup
    if (ByteUtil::BitCount(bucket_bits) == 1)
    {
        const Bucket bucket = Bucket(MathUtil::FastLog2_Pow2(bucket_bits));

        auto& mappings = render_proxy_list.mappings_by_bucket[uint32(bucket)];

        if (mappings.Empty())
        {
            return;
        }

        groups_view = { &mappings, 1 };
    }
    else
    {
        bool all_empty = true;

        for (const auto& mappings : render_proxy_list.mappings_by_bucket)
        {
            if (mappings.Any())
            {
                if (AnyOf(mappings, [&bucket_bits](const auto& it)
                        {
                            return (bucket_bits & (1u << uint32(it.first.GetMaterialAttributes().bucket))) != 0;
                        }))
                {
                    all_empty = false;

                    break;
                }
            }
        }

        if (all_empty)
        {
            return;
        }

        groups_view = render_proxy_list.mappings_by_bucket.ToSpan();
    }

    if (framebuffer)
    {
        frame->GetCommandList().Add<BeginFramebuffer>(framebuffer, frame_index);
    }

    for (const auto& mappings : groups_view)
    {
        for (const auto& it : mappings)
        {
            const RenderableAttributeSet& attributes = it.first;

            const DrawCallCollectionMapping& mapping = it.second;
            AssertDebug(mapping.IsValid());

            const Bucket bucket = attributes.GetMaterialAttributes().bucket;

            if (!(bucket_bits & (1u << uint32(bucket))))
            {
                continue;
            }

            const Handle<RenderGroup>& render_group = mapping.render_group;
            AssertDebug(render_group.IsValid());

            const DrawCallCollection& draw_call_collection = mapping.draw_call_collection;

            IndirectRenderer* indirect_renderer = mapping.indirect_renderer;

            if (push_constant)
            {
                render_group->GetPipeline()->SetPushConstants(push_constant.Data(), push_constant.Size());
            }

            ParallelRenderingState* parallel_rendering_state = nullptr;

            if (render_group->GetFlags() & RenderGroupFlags::PARALLEL_RENDERING)
            {
                parallel_rendering_state = render_proxy_list.AcquireNextParallelRenderingState();
            }

            render_group->PerformRendering(frame, render_setup, draw_call_collection, indirect_renderer, parallel_rendering_state);

            if (parallel_rendering_state != nullptr)
            {
                AssertDebug(parallel_rendering_state->task_batch != nullptr);

                TaskSystem::GetInstance().EnqueueBatch(parallel_rendering_state->task_batch);
            }
        }
    }

    // Wait for all parallel rendering tasks to finish
    render_proxy_list.CommitParallelRenderingState(frame->GetCommandList());

    if (framebuffer)
    {
        frame->GetCommandList().Add<EndFramebuffer>(framebuffer, frame_index);
    }
}

#pragma endregion RenderCollector

} // namespace hyperion