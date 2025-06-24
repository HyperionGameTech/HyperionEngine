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
#include <scene/Light.hpp>    // For LightType
#include <scene/EnvProbe.hpp> // For EnvProbeType

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
    has_forward_lighting = attributes.GetMaterialAttributes().bucket == RB_TRANSLUCENT;

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

HYP_ENABLE_OPTIMIZATION;
static void AddRenderProxy(RenderProxyList* render_proxy_list, ResourceTracker<ID<Entity>, RenderProxy>& meshes, RenderProxy* proxy, RenderView* render_view, const RenderableAttributeSet& attributes, RenderBucket rb)
{
    HYP_SCOPE;

    // Add proxy to group
    DrawCallCollectionMapping& mapping = render_proxy_list->mappings_by_bucket[rb][attributes];
    Handle<RenderGroup>& rg = mapping.render_group;

    if (!rg.IsValid())
    {
        EnumFlags<RenderGroupFlags> render_group_flags = RenderGroupFlags::DEFAULT;

        // Disable occlusion culling for translucent objects
        const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

        if (rb == RB_TRANSLUCENT || rb == RB_DEBUG)
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

    auto insert_result = mapping.render_proxies.Insert(proxy->entity.GetID(), proxy);
    volatile void* ptr = insert_result.first->second; // for debugging purposes
    AssertDebugMsg(insert_result.second,
        "RenderProxyList::AddRenderProxy: Render proxy already exists in mapping for entity #%u : render proxy entity: %u\t address: %p",
        proxy->entity.GetID().Value(),
        insert_result.first->second->entity.GetID().Value(),
        ptr);
    ptr = 0; // prevent optimization from removing the volatile pointer
}
HYP_ENABLE_OPTIMIZATION;

static bool RemoveRenderProxy(RenderProxyList* render_proxy_list, ResourceTracker<ID<Entity>, RenderProxy>& meshes, RenderProxy* proxy, const RenderableAttributeSet& attributes, RenderBucket rb)
{
    HYP_SCOPE;

    auto& mappings = render_proxy_list->mappings_by_bucket[rb];

    auto it = mappings.Find(attributes);
    AssertThrow(it != mappings.End());

    DrawCallCollectionMapping& mapping = it->second;
    AssertThrow(mapping.IsValid());

    auto proxy_iter = mapping.render_proxies.Find(proxy->entity.GetID());

    if (proxy_iter == mapping.render_proxies.End())
    {
        HYP_LOG(Rendering, Warning, "RenderProxyList::RemoveRenderProxy: Render proxy not found in mapping for entity #%u", proxy->entity.GetID().Value());
        return false;
    }

    mapping.render_proxies.Erase(proxy_iter);

    return true;
}

RenderProxyList::RenderProxyList()
    : viewport(Viewport { Vec2u::One(), Vec2i::Zero() }),
      priority(0),
      parallel_rendering_state_head(nullptr),
      parallel_rendering_state_tail(nullptr)
{
    // these are buckets per type (fixed size)
    lights.Resize(LT_MAX);
    env_probes.Resize(EPT_MAX);
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

    Clear();

#define DO_FINALIZATION_CHECK(tracker)                                                                                                    \
    AssertThrowMsg(tracker.NumCurrent() == 0,                                                                                             \
        HYP_STR(tracker) " still has %u bits set. This means that there are still render proxies that have not been removed or cleared.", \
        tracker.NumCurrent())

    DO_FINALIZATION_CHECK(meshes);
    DO_FINALIZATION_CHECK(tracked_lights);
    DO_FINALIZATION_CHECK(tracked_lightmap_volumes);
    DO_FINALIZATION_CHECK(tracked_env_grids);
    DO_FINALIZATION_CHECK(tracked_env_probes);

#undef DO_FINALIZATION_CHECK
}

void RenderProxyList::Clear()
{
    HYP_SCOPE;

    // Do not fully clear, keep the attribs around so that we can have memory reserved for each slot,
    // as well as render groups.
    for (auto& mappings : mappings_by_bucket)
    {
        for (auto& it : mappings)
        {
            DrawCallCollectionMapping& mapping = it.second;
            mapping.render_proxies.Clear();

            if (mapping.indirect_renderer)
            {
                delete mapping.indirect_renderer;
                mapping.indirect_renderer = nullptr;
            }
        }
    }
}

void RenderProxyList::RemoveEmptyRenderGroups()
{
    HYP_SCOPE;

    for (auto& mappings : mappings_by_bucket)
    {
        for (auto it = mappings.Begin(); it != mappings.End();)
        {
            DrawCallCollectionMapping& mapping = it->second;
            AssertDebug(mapping.IsValid());

            if (mapping.render_proxies.Any())
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

void RenderProxyList::BuildRenderGroups(RenderView* render_view, const RenderableAttributeSet* override_attributes)
{
    HYP_SCOPE;
    Threads::AssertOnThread(~g_render_thread);

    // should be in this state - this should be called from the game thread when the render proxy list is being built
    AssertDebug(state == CS_WRITING);

    auto diff = meshes.GetDiff();

    if (diff.NeedsUpdate())
    {
        Array<RenderProxy*> removed_proxies;
        meshes.GetRemoved(removed_proxies, true /* include_changed */);

        Array<RenderProxy*> added_proxy_ptrs;
        meshes.GetAdded(added_proxy_ptrs, true /* include_changed */);

        if (added_proxy_ptrs.Any() || removed_proxies.Any())
        {
            // Claim the added(+changed) before unclaiming the removed, as we may end up doing extra work for changed entities otherwise (unclaiming then reclaiming)
            for (RenderProxy* proxy : added_proxy_ptrs)
            {
                AssertDebug(proxy != nullptr);

                proxy->IncRefs();
            }

            for (RenderProxy* proxy : removed_proxies)
            {
                AssertDebug(proxy != nullptr);

                proxy->DecRefs();

                RenderableAttributeSet attributes = GetRenderableAttributesForProxy(*proxy, override_attributes);
                UpdateRenderableAttributesDynamic(proxy, attributes);

                const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

                AssertThrow(RemoveRenderProxy(this, meshes, proxy, attributes, rb));
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

                const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

                AddRenderProxy(this, meshes, proxy, render_view, attributes, rb);
            }
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

    if (bucket_bits == 0)
    {
        static constexpr uint32 all_buckets = (1u << RB_MAX) - 1;
        bucket_bits = all_buckets; // All buckets
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

        DrawCallCollection& draw_call_collection = mapping.draw_call_collection;

        // mapping.render_group->CollectDrawCalls(mapping.draw_call_collection);

        draw_call_collection.impl = mapping.render_group->GetDrawCallCollectionImpl();
        draw_call_collection.render_group = mapping.render_group;

        static const bool unique_per_material = g_rendering_api->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial();

        DrawCallCollection previous_draw_state = std::move(draw_call_collection);

        for (const auto& it : mapping.render_proxies)
        {
            const RenderProxy* render_proxy = it.second;
            AssertDebug(render_proxy != nullptr);

            AssertDebug(render_proxy->mesh.IsValid());
            AssertDebug(render_proxy->mesh->IsReady());

            AssertDebug(render_proxy->material.IsValid());
            AssertDebug(render_proxy->material->IsReady());

            if (render_proxy->instance_data.num_instances == 0)
            {
                continue;
            }

            DrawCallID draw_call_id;

            if (unique_per_material)
            {
                draw_call_id = DrawCallID(render_proxy->mesh->GetID(), render_proxy->material->GetID());
            }
            else
            {
                draw_call_id = DrawCallID(render_proxy->mesh->GetID());
            }

            if (!render_proxy->instance_data.enable_auto_instancing
                && render_proxy->instance_data.num_instances == 1)
            {
                draw_call_collection.PushRenderProxy(draw_call_id, *render_proxy); // NOLINT(bugprone-use-after-move)

                continue;
            }

            EntityInstanceBatch* batch = nullptr;

            // take a batch for reuse if a draw call was using one
            if ((batch = previous_draw_state.TakeDrawCallBatch(draw_call_id)) != nullptr)
            {
                const uint32 batch_index = batch->batch_index;
                AssertDebug(batch_index != ~0u);

                // Reset it
                *batch = EntityInstanceBatch { batch_index };

                draw_call_collection.impl->GetEntityInstanceBatchHolder()->MarkDirty(batch->batch_index);
            }

            draw_call_collection.PushRenderProxyInstanced(batch, draw_call_id, *render_proxy);
        }

        // Any draw calls that were not reused from the previous state, clear them out and release batch indices.
        previous_draw_state.ResetDrawCalls();
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
        const RenderBucket rb = RenderBucket(MathUtil::FastLog2_Pow2(bucket_bits));

        auto& mappings = render_proxy_list.mappings_by_bucket[rb];

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

            const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

            if (!(bucket_bits & (1u << uint32(rb))))
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