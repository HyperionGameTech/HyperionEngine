/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderGroup.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderSkeleton.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/GraphicsPipelineCache.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <scene/Entity.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/utilities/ForEach.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>
#include <Constants.hpp>

namespace hyperion {

using renderer::CommandBufferType;

#pragma region RenderGroup

RenderGroup::RenderGroup()
    : HypObject(),
      m_flags(RenderGroupFlags::NONE),
      m_draw_state(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>(), this)
{
}

RenderGroup::RenderGroup(
    const ShaderRef& shader,
    const RenderableAttributeSet& renderable_attributes,
    EnumFlags<RenderGroupFlags> flags)
    : HypObject(),
      m_flags(flags),
      m_shader(shader),
      m_renderable_attributes(renderable_attributes),
      m_draw_state(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>(), this)
{
}

RenderGroup::RenderGroup(
    const ShaderRef& shader,
    const RenderableAttributeSet& renderable_attributes,
    const DescriptorTableRef& descriptor_table,
    EnumFlags<RenderGroupFlags> flags)
    : HypObject(),
      m_flags(flags),
      m_shader(shader),
      m_descriptor_table(descriptor_table),
      m_renderable_attributes(renderable_attributes),
      m_draw_state(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>(), this)
{
}

RenderGroup::~RenderGroup()
{
    // for (auto &it : m_render_proxies) {
    //     it.second->DecRefs();
    // }

    if (m_indirect_renderer != nullptr)
    {
        m_indirect_renderer->Destroy();
    }

    SafeRelease(std::move(m_shader));
    SafeRelease(std::move(m_descriptor_table));
    SafeRelease(std::move(m_fbos));
    SafeRelease(std::move(m_pipeline));
}

void RenderGroup::SetShader(const ShaderRef& shader)
{
    HYP_SCOPE;

    SafeRelease(std::move(m_shader));

    m_shader = shader;

    // @TODO Re-create pipeline
}

void RenderGroup::SetRenderableAttributes(const RenderableAttributeSet& renderable_attributes)
{
    m_renderable_attributes = renderable_attributes;
}

void RenderGroup::AddFramebuffer(const FramebufferRef& framebuffer)
{
    HYP_SCOPE;

    const auto it = m_fbos.Find(framebuffer);

    if (it != m_fbos.End())
    {
        return;
    }

    m_fbos.PushBack(framebuffer);

    // @TODO Re-create pipeline
}

void RenderGroup::RemoveFramebuffer(const FramebufferRef& framebuffer)
{
    HYP_SCOPE;

    const auto it = m_fbos.Find(framebuffer);

    if (it == m_fbos.End())
    {
        return;
    }

    m_fbos.Erase(it);
}

void RenderGroup::Init()
{
    HYP_SCOPE;

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
        {
            HYP_SCOPE;

            if (m_indirect_renderer != nullptr)
            {
                m_indirect_renderer->Destroy();
            }

            SafeRelease(std::move(m_shader));
            SafeRelease(std::move(m_descriptor_table));
            SafeRelease(std::move(m_fbos));
            SafeRelease(std::move(m_pipeline));
        }));

    // If parallel rendering is globally disabled, disable it for this RenderGroup
    if (!g_rendering_api->GetRenderConfig().IsParallelRenderingEnabled())
    {
        m_flags &= ~RenderGroupFlags::PARALLEL_RENDERING;
    }

    if (!g_rendering_api->GetRenderConfig().IsIndirectRenderingEnabled())
    {
        m_flags &= ~RenderGroupFlags::INDIRECT_RENDERING;
    }

    CreateIndirectRenderer();
    CreateGraphicsPipeline();

    SetReady(true);
}

void RenderGroup::CreateIndirectRenderer()
{
    HYP_SCOPE;

    if (m_flags & RenderGroupFlags::INDIRECT_RENDERING)
    {
        m_indirect_renderer = MakeUnique<IndirectRenderer>(&m_draw_state);
        m_indirect_renderer->Create();
    }
}

void RenderGroup::CreateGraphicsPipeline()
{
    HYP_SCOPE;

    AssertThrowMsg(m_fbos.Any(), "No framebuffers attached to RenderGroup");

    for (const FramebufferRef& framebuffer : m_fbos)
    {
        AssertThrow(framebuffer.IsValid());

        DeferCreate(framebuffer);
    }

    AssertThrow(m_shader.IsValid());

    if (!m_descriptor_table.IsValid())
    {
        const renderer::DescriptorTableDeclaration& descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        m_descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);
        m_descriptor_table->SetDebugName(NAME_FMT("DescriptorTable_{}", m_shader->GetCompiledShader()->GetName()));

        // Setup instancing buffers if "Instancing" descriptor set exists
        const uint32 instancing_descriptor_set_index = m_descriptor_table->GetDescriptorSetIndex(NAME("Instancing"));

        if (instancing_descriptor_set_index != ~0u)
        {
            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
            {
                const GPUBufferRef& gpu_buffer = m_draw_state.GetImpl()->GetEntityInstanceBatchHolder()->GetBuffer(frame_index);
                AssertThrow(gpu_buffer.IsValid());

                const DescriptorSetRef& instancing_descriptor_set = m_descriptor_table->GetDescriptorSet(NAME("Instancing"), frame_index);
                AssertThrow(instancing_descriptor_set.IsValid());

                instancing_descriptor_set->SetElement(NAME("EntityInstanceBatchesBuffer"), gpu_buffer);
            }
        }

        DeferCreate(m_descriptor_table);
    }

    AssertThrow(m_descriptor_table.IsValid());

    m_pipeline = g_engine->GetGraphicsPipelineCache()->GetOrCreate(
        m_shader,
        m_descriptor_table,
        m_fbos,
        m_renderable_attributes);
}

void RenderGroup::ClearProxies()
{
    Threads::AssertOnThread(g_render_thread);

    // for (auto &it : m_render_proxies) {
    //     it.second->DecRefs();
    // }

    m_render_proxies.Clear();
}

void RenderGroup::AddRenderProxy(RenderProxy* render_proxy)
{
    AssertDebug(render_proxy != nullptr);

    Threads::AssertOnThread(g_render_thread);

    AssertDebug(render_proxy->mesh.IsValid());
    AssertDebug(render_proxy->mesh->IsReady());

    AssertDebug(render_proxy->material.IsValid());
    AssertDebug(render_proxy->material->IsReady());

    bool inserted = m_render_proxies.Insert(render_proxy->entity.GetID(), render_proxy).second;
    AssertDebug(inserted);
}

bool RenderGroup::RemoveRenderProxy(ID<Entity> entity)
{
    Threads::AssertOnThread(g_render_thread);

    const auto it = m_render_proxies.Find(entity);

    if (it == m_render_proxies.End())
    {
        return false;
    }

    // it->second->DecRefs();

    m_render_proxies.Erase(it);

    return true;
}

typename FlatMap<ID<Entity>, const RenderProxy*>::Iterator RenderGroup::RemoveRenderProxy(typename FlatMap<ID<Entity>, const RenderProxy*>::ConstIterator iterator)
{
    Threads::AssertOnThread(g_render_thread);

    return m_render_proxies.Erase(iterator);
}

void RenderGroup::SetDrawCallCollectionImpl(IDrawCallCollectionImpl* draw_call_collection_impl)
{
    AssertThrow(draw_call_collection_impl != nullptr);

    AssertThrowMsg(!IsInitCalled(), "Cannot use SetDrawCallCollectionImpl() after Init() has been called on RenderGroup; graphics pipeline will have been already created");

    m_draw_state = DrawCallCollection(draw_call_collection_impl, this);
}

void RenderGroup::CollectDrawCalls()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    static const bool unique_per_material = g_rendering_api->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial();

    if (m_flags & RenderGroupFlags::INDIRECT_RENDERING)
    {
        m_indirect_renderer->GetDrawState().ResetDrawState();
    }

    DrawCallCollection previous_draw_state = std::move(m_draw_state);

    for (const auto& it : m_render_proxies)
    {
        const RenderProxy* render_proxy = it.second;

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
            m_draw_state.PushRenderProxy(draw_call_id, *render_proxy);

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

            m_draw_state.GetImpl()->GetEntityInstanceBatchHolder()->MarkDirty(batch->batch_index);
        }

        m_draw_state.PushRenderProxyInstanced(batch, draw_call_id, *render_proxy);
    }

    previous_draw_state.ResetDrawCalls();

    // register draw calls for indirect rendering
    if (m_flags & RenderGroupFlags::INDIRECT_RENDERING)
    {
        m_indirect_renderer->PushDrawCallsToIndirectState();
    }
}

void RenderGroup::PerformOcclusionCulling(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;

    static const bool is_indirect_rendering_enabled = g_rendering_api->GetRenderConfig().IsIndirectRenderingEnabled();

    if (!is_indirect_rendering_enabled)
    {
        return;
    }

    AssertThrow((m_flags & (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING)) == (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING));

    Threads::AssertOnThread(g_render_thread);

    m_indirect_renderer->ExecuteCullShaderInBatches(frame, render_setup);
}

template <class T, class OutArray, typename = std::enable_if_t<std::is_base_of_v<DrawCallBase, T>>>
static void DivideDrawCalls(Span<const T> draw_calls, uint32 num_batches, OutArray& out_divided_draw_calls)
{
    HYP_SCOPE;

    out_divided_draw_calls.Clear();

    const uint32 num_draw_calls = uint32(draw_calls.Size());

    // Make sure we don't try to divide into more batches than we have draw calls
    num_batches = MathUtil::Min(num_batches, num_draw_calls);
    out_divided_draw_calls.Resize(num_batches);

    const uint32 num_draw_calls_divided = (num_draw_calls + num_batches - 1) / num_batches;

    uint32 draw_call_index = 0;

    for (uint32 container_index = 0; container_index < num_batches; container_index++)
    {
        const uint32 diff_to_next_or_end = MathUtil::Min(num_draw_calls_divided, num_draw_calls - draw_call_index);

        out_divided_draw_calls[container_index] = {
            draw_calls.Begin() + draw_call_index,
            draw_calls.Begin() + draw_call_index + diff_to_next_or_end
        };

        // sanity check
        AssertThrow(draw_calls.Size() >= draw_call_index + diff_to_next_or_end);

        draw_call_index += diff_to_next_or_end;
    }
}

template <bool UseIndirectRendering>
static void RenderAll(
    FrameBase* frame,
    const RenderSetup& render_setup,
    const GraphicsPipelineRef& pipeline,
    IndirectRenderer* indirect_renderer,
    const DrawCallCollection& draw_state)
{
    HYP_SCOPE;

    static const bool use_bindless_textures = g_rendering_api->GetRenderConfig().IsBindlessSupported();

    if (draw_state.GetInstancedDrawCalls().Size() == 0 && draw_state.GetDrawCalls().Size() == 0)
    {
        // No draw calls to render
        return;
    }

    const TResourceHandle<RenderLight>& render_light = g_engine->GetRenderState()->GetActiveLight();

    const uint32 frame_index = frame->GetFrameIndex();

    const uint32 global_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
    const DescriptorSetRef& global_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index);

    const uint32 view_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

    const uint32 material_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Material"));
    const DescriptorSetRef& material_descriptor_set = use_bindless_textures
        ? pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Material"), frame_index)
        : DescriptorSetRef::unset;

    const uint32 entity_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Object"));
    const DescriptorSetRef& entity_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Object"), frame_index);

    const uint32 instancing_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Instancing"));
    const DescriptorSetRef& instancing_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Instancing"), frame_index);

    EngineRenderStatsCounts counts;

    frame->GetCommandList().Add<BindGraphicsPipeline>(pipeline);

    if (global_descriptor_set_index != ~0u)
    {
        frame->GetCommandList().Add<BindDescriptorSet>(
            global_descriptor_set,
            pipeline,
            ArrayMap<Name, uint32> {
                { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) },
                { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(render_setup.env_grid, 0) },
                { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_light.Get(), 0) },
                { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } },
            global_descriptor_set_index);
    }

    if (view_descriptor_set_index != ~0u)
    {
        AssertThrow(render_setup.HasView());

        const DescriptorSetRef& view_descriptor_set = render_setup.view->GetDescriptorSets()[frame_index];

        AssertThrowMsg(view_descriptor_set.IsValid(), "Scene descriptor set is not valid but should be (shader: %s, scene descriptor set index: %u)",
            pipeline->GetShader()->GetCompiledShader()->GetDefinition().GetName().LookupString(), view_descriptor_set_index);

        frame->GetCommandList().Add<BindDescriptorSet>(
            view_descriptor_set,
            pipeline,
            ArrayMap<Name, uint32> {},
            view_descriptor_set_index);
    }

    // Bind textures globally (bindless)
    if (material_descriptor_set_index != ~0u && use_bindless_textures)
    {
        frame->GetCommandList().Add<BindDescriptorSet>(
            material_descriptor_set,
            pipeline,
            ArrayMap<Name, uint32> {},
            material_descriptor_set_index);
    }

    for (const DrawCall& draw_call : draw_state.GetDrawCalls())
    {
        if (entity_descriptor_set.IsValid())
        {
            ArrayMap<Name, uint32> offsets;
            offsets[NAME("SkeletonsBuffer")] = ShaderDataOffset<SkeletonShaderData>(draw_call.render_skeleton != nullptr ? draw_call.render_skeleton->GetBufferIndex() : 0);
            offsets[NAME("CurrentObject")] = ShaderDataOffset<EntityShaderData>(draw_call.entity_id.ToIndex());

            if (g_rendering_api->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial())
            {
                offsets[NAME("MaterialsBuffer")] = ShaderDataOffset<MaterialShaderData>(draw_call.render_material != nullptr ? draw_call.render_material->GetBufferIndex() : 0);
            }

            frame->GetCommandList().Add<BindDescriptorSet>(
                entity_descriptor_set,
                pipeline,
                offsets,
                entity_descriptor_set_index);
        }

        // Bind material descriptor set
        if (material_descriptor_set_index != ~0u && !use_bindless_textures)
        {
            const DescriptorSetRef& material_descriptor_set = draw_call.render_material->GetDescriptorSets()[frame_index];
            AssertThrow(material_descriptor_set.IsValid());

            frame->GetCommandList().Add<BindDescriptorSet>(
                material_descriptor_set,
                pipeline,
                ArrayMap<Name, uint32> {},
                material_descriptor_set_index);
        }

        if (UseIndirectRendering && draw_call.draw_command_index != ~0u)
        {
            draw_call.render_mesh->RenderIndirect(
                frame->GetCommandList(),
                indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index),
                draw_call.draw_command_index * uint32(sizeof(IndirectDrawCommand)));
        }
        else
        {
            draw_call.render_mesh->Render(frame->GetCommandList(), 1);
        }

        counts[ERS_DRAW_CALLS]++;
        counts[ERS_TRIANGLES] += draw_call.render_mesh->NumIndices() / 3;
    }

    for (const InstancedDrawCall& draw_call : draw_state.GetInstancedDrawCalls())
    {
        EntityInstanceBatch* entity_instance_batch = draw_call.batch;
        AssertDebug(entity_instance_batch != nullptr);

        AssertThrow(instancing_descriptor_set.IsValid());

        if (entity_descriptor_set.IsValid())
        {
            ArrayMap<Name, uint32> offsets;
            offsets[NAME("SkeletonsBuffer")] = ShaderDataOffset<SkeletonShaderData>(draw_call.render_skeleton != nullptr ? draw_call.render_skeleton->GetBufferIndex() : 0);

            if (g_rendering_api->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial())
            {
                offsets[NAME("MaterialsBuffer")] = ShaderDataOffset<MaterialShaderData>(draw_call.render_material != nullptr ? draw_call.render_material->GetBufferIndex() : 0);
            }

            frame->GetCommandList().Add<BindDescriptorSet>(
                entity_descriptor_set,
                pipeline,
                offsets,
                entity_descriptor_set_index);
        }

        const SizeType offset = entity_instance_batch->batch_index * draw_state.GetImpl()->GetBatchSizeOf();

        frame->GetCommandList().Add<BindDescriptorSet>(
            instancing_descriptor_set,
            pipeline,
            ArrayMap<Name, uint32> {
                { NAME("EntityInstanceBatchesBuffer"), uint32(offset) } },
            instancing_descriptor_set_index);

        // Bind material descriptor set
        if (material_descriptor_set_index != ~0u && !use_bindless_textures)
        {
            const DescriptorSetRef& material_descriptor_set = draw_call.render_material->GetDescriptorSets()[frame_index];
            AssertThrow(material_descriptor_set.IsValid());

            frame->GetCommandList().Add<BindDescriptorSet>(
                material_descriptor_set,
                pipeline,
                ArrayMap<Name, uint32> {},
                material_descriptor_set_index);
        }

        if (UseIndirectRendering && draw_call.draw_command_index != ~0u)
        {
            draw_call.render_mesh->RenderIndirect(
                frame->GetCommandList(),
                indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index),
                draw_call.draw_command_index * uint32(sizeof(IndirectDrawCommand)));
        }
        else
        {
            draw_call.render_mesh->Render(frame->GetCommandList(), entity_instance_batch->num_entities);
        }

        counts[ERS_DRAW_CALLS]++;
        counts[ERS_INSTANCED_DRAW_CALLS]++;
        counts[ERS_TRIANGLES] += draw_call.render_mesh->NumIndices() / 3;
    }

    g_engine->GetRenderStatsCalculator().AddCounts(counts);
}

template <bool UseIndirectRendering>
static void RenderAll_Parallel(
    FrameBase* frame,
    const RenderSetup& render_setup,
    const GraphicsPipelineRef& pipeline,
    IndirectRenderer* indirect_renderer,
    const DrawCallCollection& draw_state,
    ParallelRenderingState* parallel_rendering_state)
{
    HYP_SCOPE;

    AssertDebug(parallel_rendering_state != nullptr);

    static const bool use_bindless_textures = g_rendering_api->GetRenderConfig().IsBindlessSupported();

    if (draw_state.GetInstancedDrawCalls().Size() == 0 && draw_state.GetDrawCalls().Size() == 0)
    {
        // No draw calls to render
        return;
    }

    const TResourceHandle<RenderLight>& render_light = g_engine->GetRenderState()->GetActiveLight();

    const uint32 frame_index = frame->GetFrameIndex();

    const uint32 global_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
    const DescriptorSetRef& global_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index);

    const uint32 view_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));
    const uint32 material_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Material"));

    RHICommandList& base_command_list = parallel_rendering_state->base_command_list;

    base_command_list.Add<BindGraphicsPipeline>(pipeline);

    if (global_descriptor_set_index != ~0u)
    {
        base_command_list.Add<BindDescriptorSet>(
            global_descriptor_set,
            pipeline,
            ArrayMap<Name, uint32> {
                { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) },
                { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(render_setup.env_grid, 0) },
                { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_light.Get(), 0) },
                { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } },
            global_descriptor_set_index);
    }

    if (view_descriptor_set_index != ~0u)
    {
        AssertThrow(render_setup.HasView());

        const DescriptorSetRef& view_descriptor_set = render_setup.view->GetDescriptorSets()[frame_index];

        AssertThrowMsg(view_descriptor_set.IsValid(), "Scene descriptor set is not valid but should be (shader: %s, scene descriptor set index: %u)",
            pipeline->GetShader()->GetCompiledShader()->GetDefinition().GetName().LookupString(), view_descriptor_set_index);

        base_command_list.Add<BindDescriptorSet>(
            view_descriptor_set,
            pipeline,
            ArrayMap<Name, uint32> {},
            view_descriptor_set_index);
    }

    // Bind textures globally (bindless)
    if (material_descriptor_set_index != ~0u && use_bindless_textures)
    {
        const DescriptorSetRef& material_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Material"), frame_index);
        AssertDebug(material_descriptor_set.IsValid());

        base_command_list.Add<BindDescriptorSet>(
            material_descriptor_set,
            pipeline,
            ArrayMap<Name, uint32> {},
            material_descriptor_set_index);
    }

    // Store the proc in the parallel rendering state so that it doesn't get destroyed until we're done with it
    if (draw_state.GetDrawCalls().Size() != 0)
    {
        DivideDrawCalls(draw_state.GetDrawCalls(), parallel_rendering_state->num_batches, parallel_rendering_state->draw_calls);

        ProcRef<void(Span<const DrawCall>, uint32, uint32)> proc = parallel_rendering_state->draw_call_procs.EmplaceBack([frame_index, parallel_rendering_state, &draw_state, &pipeline, indirect_renderer, material_descriptor_set_index](Span<const DrawCall> draw_calls, uint32 index, uint32)
            {
                if (!draw_calls)
                {
                    return;
                }

                RHICommandList& command_list = parallel_rendering_state->command_lists[index];

                const uint32 entity_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Object"));
                const DescriptorSetRef& entity_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Object"), frame_index);

                for (const DrawCall& draw_call : draw_calls)
                {
                    if (entity_descriptor_set.IsValid())
                    {
                        ArrayMap<Name, uint32> offsets;
                        offsets[NAME("SkeletonsBuffer")] = ShaderDataOffset<SkeletonShaderData>(draw_call.render_skeleton != nullptr ? draw_call.render_skeleton->GetBufferIndex() : 0);
                        offsets[NAME("CurrentObject")] = ShaderDataOffset<EntityShaderData>(draw_call.entity_id.ToIndex());

                        if (g_rendering_api->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial())
                        {
                            offsets[NAME("MaterialsBuffer")] = ShaderDataOffset<MaterialShaderData>(draw_call.render_material != nullptr ? draw_call.render_material->GetBufferIndex() : 0);
                        }

                        command_list.Add<BindDescriptorSet>(
                            entity_descriptor_set,
                            pipeline,
                            offsets,
                            entity_descriptor_set_index);
                    }

                    // Bind material descriptor set
                    if (material_descriptor_set_index != ~0u && !use_bindless_textures)
                    {
                        const DescriptorSetRef& material_descriptor_set = draw_call.render_material->GetDescriptorSets()[frame_index];
                        AssertDebug(material_descriptor_set.IsValid());

                        command_list.Add<BindDescriptorSet>(
                            material_descriptor_set,
                            pipeline,
                            ArrayMap<Name, uint32> {},
                            material_descriptor_set_index);
                    }

                    if (UseIndirectRendering && draw_call.draw_command_index != ~0u)
                    {
                        draw_call.render_mesh->RenderIndirect(
                            command_list,
                            indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index),
                            draw_call.draw_command_index * uint32(sizeof(IndirectDrawCommand)));
                    }
                    else
                    {
                        draw_call.render_mesh->Render(command_list, 1);
                    }

                    parallel_rendering_state->render_stats_counts[index][ERS_TRIANGLES] += draw_call.render_mesh->NumIndices() / 3;
                    parallel_rendering_state->render_stats_counts[index][ERS_DRAW_CALLS]++;
                }
            });

        TaskSystem::GetInstance().ParallelForEach_Batch(
            *parallel_rendering_state->task_batch,
            parallel_rendering_state->num_batches,
            parallel_rendering_state->draw_calls,
            std::move(proc));
    }

    if (draw_state.GetInstancedDrawCalls().Size() != 0)
    {
        DivideDrawCalls(draw_state.GetInstancedDrawCalls(), parallel_rendering_state->num_batches, parallel_rendering_state->instanced_draw_calls);

        ProcRef<void(Span<const InstancedDrawCall>, uint32, uint32)> proc = parallel_rendering_state->instanced_draw_call_procs.EmplaceBack([frame_index, parallel_rendering_state, &draw_state, &pipeline, indirect_renderer, material_descriptor_set_index](Span<const InstancedDrawCall> draw_calls, uint32 index, uint32)
            {
                if (!draw_calls)
                {
                    return;
                }

                RHICommandList& command_list = parallel_rendering_state->command_lists[index];

                const uint32 entity_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Object"));
                const DescriptorSetRef& entity_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Object"), frame_index);

                const uint32 instancing_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Instancing"));
                const DescriptorSetRef& instancing_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Instancing"), frame_index);

                AssertDebug(instancing_descriptor_set.IsValid());

                for (const InstancedDrawCall& draw_call : draw_calls)
                {
                    // @TODO: Skip using an EntityInstanceBatch if the draw call is not instanced
                    EntityInstanceBatch* entity_instance_batch = draw_call.batch;
                    AssertDebug(entity_instance_batch != nullptr);

                    if (entity_descriptor_set.IsValid())
                    {
                        ArrayMap<Name, uint32> offsets;
                        offsets[NAME("SkeletonsBuffer")] = ShaderDataOffset<SkeletonShaderData>(draw_call.render_skeleton != nullptr ? draw_call.render_skeleton->GetBufferIndex() : 0);

                        if (g_rendering_api->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial())
                        {
                            offsets[NAME("MaterialsBuffer")] = ShaderDataOffset<MaterialShaderData>(draw_call.render_material != nullptr ? draw_call.render_material->GetBufferIndex() : 0);
                        }

                        command_list.Add<BindDescriptorSet>(
                            entity_descriptor_set,
                            pipeline,
                            offsets,
                            entity_descriptor_set_index);
                    }

                    const SizeType offset = entity_instance_batch->batch_index * draw_state.GetImpl()->GetBatchSizeOf();

                    command_list.Add<BindDescriptorSet>(
                        instancing_descriptor_set,
                        pipeline,
                        ArrayMap<Name, uint32> {
                            { NAME("EntityInstanceBatchesBuffer"), uint32(offset) } },
                        instancing_descriptor_set_index);

                    // Bind material descriptor set
                    if (material_descriptor_set_index != ~0u && !use_bindless_textures)
                    {
                        const DescriptorSetRef& material_descriptor_set = draw_call.render_material->GetDescriptorSets()[frame_index];
                        AssertDebug(material_descriptor_set.IsValid());

                        command_list.Add<BindDescriptorSet>(
                            material_descriptor_set,
                            pipeline,
                            ArrayMap<Name, uint32> {},
                            material_descriptor_set_index);
                    }

                    if (UseIndirectRendering && draw_call.draw_command_index != ~0u)
                    {
                        draw_call.render_mesh->RenderIndirect(
                            command_list,
                            indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index),
                            draw_call.draw_command_index * uint32(sizeof(IndirectDrawCommand)));
                    }
                    else
                    {
                        draw_call.render_mesh->Render(command_list, entity_instance_batch->num_entities);
                    }

                    parallel_rendering_state->render_stats_counts[index][ERS_TRIANGLES] += draw_call.render_mesh->NumIndices() / 3;
                    parallel_rendering_state->render_stats_counts[index][ERS_DRAW_CALLS]++;
                    parallel_rendering_state->render_stats_counts[index][ERS_INSTANCED_DRAW_CALLS]++;
                }
            });

        TaskSystem::GetInstance().ParallelForEach_Batch(
            *parallel_rendering_state->task_batch,
            parallel_rendering_state->num_batches,
            parallel_rendering_state->instanced_draw_calls,
            std::move(proc));
    }
}

void RenderGroup::PerformRendering(FrameBase* frame, const RenderSetup& render_setup, ParallelRenderingState* parallel_rendering_state)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);
    AssertReady();

    AssertDebugMsg(render_setup.IsValid(), "RenderSetup must be valid for rendering");
    AssertDebugMsg(render_setup.HasView(), "RenderSetup must have a valid RenderView for rendering");

    static const bool is_indirect_rendering_enabled = g_rendering_api->GetRenderConfig().IsIndirectRenderingEnabled();

    const bool use_indirect_rendering = is_indirect_rendering_enabled
        && m_flags[RenderGroupFlags::INDIRECT_RENDERING]
        && render_setup.view->GetCullData().depth_pyramid_image_view != nullptr;

    if (use_indirect_rendering)
    {
        if (m_flags & RenderGroupFlags::PARALLEL_RENDERING)
        {
            RenderAll_Parallel<true>(
                frame,
                render_setup,
                m_pipeline,
                m_indirect_renderer.Get(),
                m_draw_state,
                parallel_rendering_state);
        }
        else
        {
            RenderAll<true>(
                frame,
                render_setup,
                m_pipeline,
                m_indirect_renderer.Get(),
                m_draw_state);
        }
    }
    else
    {
        if (m_flags & RenderGroupFlags::PARALLEL_RENDERING)
        {
            AssertDebug(parallel_rendering_state != nullptr);

            RenderAll_Parallel<false>(
                frame,
                render_setup,
                m_pipeline,
                m_indirect_renderer.Get(),
                m_draw_state,
                parallel_rendering_state);
        }
        else
        {
            RenderAll<false>(
                frame,
                render_setup,
                m_pipeline,
                m_indirect_renderer.Get(),
                m_draw_state);
        }
    }

    EngineRenderStatsCounts counts;
    counts[ERS_RENDER_GROUPS] = 1;

    g_engine->GetRenderStatsCalculator().AddCounts(counts);
}

#pragma endregion RenderGroup

} // namespace hyperion
