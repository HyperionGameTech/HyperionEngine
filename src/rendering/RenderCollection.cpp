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

    HYP_MT_CHECK_READ(m_data_race_detector);

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

    static const bool is_indirect_rendering_enabled = g_rendering_api->GetRenderConfig().IsIndirectRenderingEnabled();
    const bool perform_occlusion_culling = is_indirect_rendering_enabled && render_setup.view->GetCullData().depth_pyramid_image_view != nullptr;

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

    const FramebufferRef& framebuffer = render_setup.view->GetView()->GetOutputTarget();
    AssertDebugMsg(framebuffer, "View has no Framebuffer attached");

    ExecuteDrawCalls(frame, render_setup, render_proxy_list, framebuffer, bucket_bits, push_constant);
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