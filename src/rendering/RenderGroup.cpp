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
#include <rendering/RenderCollection.hpp>
#include <rendering/GraphicsPipelineCache.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RenderConfig.hpp>

// temp
#include <rendering/backend/vulkan/RendererGraphicsPipeline.hpp>
#include <rendering/backend/vulkan/RendererRenderPass.hpp>
#include <rendering/backend/vulkan/RendererFramebuffer.hpp>

#include <scene/Entity.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/utilities/ForEach.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/Defines.hpp>

#include <Engine.hpp>
#include <Constants.hpp>

namespace hyperion {

#pragma region RenderGroup

RenderGroup::RenderGroup()
    : HypObject(),
      m_flags(RenderGroupFlags::NONE),
      m_draw_call_collection_impl(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>())
{
}

RenderGroup::RenderGroup(const ShaderRef& shader, const RenderableAttributeSet& renderable_attributes, EnumFlags<RenderGroupFlags> flags)
    : HypObject(),
      m_flags(flags),
      m_shader(shader),
      m_renderable_attributes(renderable_attributes),
      m_draw_call_collection_impl(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>())
{
}

RenderGroup::RenderGroup(const ShaderRef& shader, const RenderableAttributeSet& renderable_attributes, const DescriptorTableRef& descriptor_table, EnumFlags<RenderGroupFlags> flags)
    : HypObject(),
      m_flags(flags),
      m_shader(shader),
      m_descriptor_table(descriptor_table),
      m_renderable_attributes(renderable_attributes),
      m_draw_call_collection_impl(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>())
{
}

RenderGroup::~RenderGroup()
{
    // for (auto &it : m_render_proxies) {
    //     it.second->DecRefs();
    // }

    SafeRelease(std::move(m_shader));
    SafeRelease(std::move(m_descriptor_table));
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

void RenderGroup::Init()
{
    HYP_SCOPE;

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
        {
            HYP_SCOPE;

            SafeRelease(std::move(m_shader));
            SafeRelease(std::move(m_descriptor_table));
        }));

    // If parallel rendering is globally disabled, disable it for this RenderGroup
    if (!g_render_backend->GetRenderConfig().IsParallelRenderingEnabled())
    {
        m_flags &= ~RenderGroupFlags::PARALLEL_RENDERING;
    }

    if (!g_render_backend->GetRenderConfig().IsIndirectRenderingEnabled())
    {
        m_flags &= ~RenderGroupFlags::INDIRECT_RENDERING;
    }

    SetReady(true);
}

GraphicsPipelineRef RenderGroup::CreateGraphicsPipeline(PassData* pd) const
{
    HYP_SCOPE;

    AssertThrow(pd != nullptr);

    HYP_LOG(Rendering, Debug, "Creating graphics pipeline for RenderGroup: {}", Id());

    Handle<View> view = pd->view.Lock();
    AssertThrow(view.IsValid());
    AssertThrow(view->GetOutputTarget().IsValid());

    AssertThrow(m_shader.IsValid());

    DescriptorTableRef descriptor_table = m_descriptor_table;

    if (!descriptor_table.IsValid())
    {
        const DescriptorTableDeclaration& descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        descriptor_table = g_render_backend->MakeDescriptorTable(&descriptor_table_decl);
        descriptor_table->SetDebugName(NAME_FMT("DescriptorTable_{}", m_shader->GetCompiledShader()->GetName()));

        // Setup instancing buffers if "Instancing" descriptor set exists
        const uint32 instancing_descriptor_set_index = descriptor_table->GetDescriptorSetIndex(NAME("Instancing"));

        if (instancing_descriptor_set_index != ~0u)
        {
            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
            {
                const GpuBufferRef& gpu_buffer = m_draw_call_collection_impl->GetEntityInstanceBatchHolder()->GetBuffer(frame_index);
                AssertThrow(gpu_buffer.IsValid());

                const DescriptorSetRef& instancing_descriptor_set = descriptor_table->GetDescriptorSet(NAME("Instancing"), frame_index);
                AssertThrow(instancing_descriptor_set.IsValid());

                instancing_descriptor_set->SetElement(NAME("EntityInstanceBatchesBuffer"), gpu_buffer);
            }
        }

        DeferCreate(descriptor_table);
    }

    AssertThrow(descriptor_table.IsValid());

    return g_engine->GetGraphicsPipelineCache()->GetOrCreate(
        m_shader,
        descriptor_table,
        { &view->GetOutputTarget().GetFramebuffer(m_renderable_attributes.GetMaterialAttributes().bucket), 1 },
        m_renderable_attributes);
}

void RenderGroup::SetDrawCallCollectionImpl(IDrawCallCollectionImpl* draw_call_collection_impl)
{
    AssertThrow(draw_call_collection_impl != nullptr);

    AssertThrowMsg(!IsInitCalled(), "Cannot use SetDrawCallCollectionImpl() after Init() has been called on RenderGroup; graphics pipeline will have been already created");

    m_draw_call_collection_impl = draw_call_collection_impl;
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

static void ValidatePipelineState(const RenderSetup& render_setup, const GraphicsPipelineRef& pipeline)
{
#if 0
    HYP_SCOPE;

    AssertThrow(render_setup.IsValid());
    AssertThrow(pipeline.IsValid());

    AssertThrow(render_setup.pass_data != nullptr);

    const Handle<View> view = render_setup.pass_data->view.Lock();
    AssertThrow(view.IsValid());

    const ViewOutputTarget& output_target = view->GetOutputTarget();
    AssertThrow(output_target.IsValid());

    // Pipeline state validation: Does the pipeline framebuffer match the output target?
    const Array<FramebufferRef>& pipeline_framebuffers = pipeline->GetFramebuffers();

    for (uint32 i = 0; i < pipeline_framebuffers.Size(); ++i)
    {
        AssertDebugMsg(pipeline_framebuffers[i] == output_target.GetFramebuffers()[i],
            "Pipeline framebuffer at index {} does not match output target framebuffer at index {}",
            i, i);
    }
#endif
}

template <bool UseIndirectRendering>
static void RenderAll(
    FrameBase* frame,
    const RenderSetup& render_setup,
    const GraphicsPipelineRef& pipeline,
    IndirectRenderer* indirect_renderer,
    const DrawCallCollection& draw_call_collection)
{
    HYP_SCOPE;

    if constexpr (UseIndirectRendering)
    {
        AssertDebug(indirect_renderer != nullptr);
    }

    static const bool use_bindless_textures = g_render_backend->GetRenderConfig().IsBindlessSupported();

    if (draw_call_collection.instanced_draw_calls.Empty() && draw_call_collection.draw_calls.Empty())
    {
        // No draw calls to render
        return;
    }

    ValidatePipelineState(render_setup, pipeline);

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
                { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(render_setup.env_grid ? render_setup.env_grid->GetRenderResource().GetBufferIndex() : 0) },
                { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_setup.light ? render_setup.light->GetRenderResource().GetBufferIndex() : 0) },
                { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe ? render_setup.env_probe->GetRenderResource().GetBufferIndex() : 0) } },
            global_descriptor_set_index);
    }

    if (view_descriptor_set_index != ~0u)
    {
        AssertThrow(render_setup.pass_data != nullptr);

        frame->GetCommandList().Add<BindDescriptorSet>(
            render_setup.pass_data->descriptor_sets[frame_index],
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

    for (const DrawCall& draw_call : draw_call_collection.draw_calls)
    {
        if (entity_descriptor_set.IsValid())
        {
            ArrayMap<Name, uint32> offsets;
            offsets[NAME("SkeletonsBuffer")] = ShaderDataOffset<SkeletonShaderData>(draw_call.render_skeleton != nullptr ? draw_call.render_skeleton->GetBufferIndex() : 0);
            offsets[NAME("CurrentObject")] = ShaderDataOffset<EntityShaderData>(draw_call.entity_id.ToIndex());

            if (g_render_backend->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial())
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

    for (const InstancedDrawCall& draw_call : draw_call_collection.instanced_draw_calls)
    {
        EntityInstanceBatch* entity_instance_batch = draw_call.batch;
        AssertDebug(entity_instance_batch != nullptr);

        AssertThrow(instancing_descriptor_set.IsValid());

        if (entity_descriptor_set.IsValid())
        {
            ArrayMap<Name, uint32> offsets;
            offsets[NAME("SkeletonsBuffer")] = ShaderDataOffset<SkeletonShaderData>(draw_call.render_skeleton != nullptr ? draw_call.render_skeleton->GetBufferIndex() : 0);

            if (g_render_backend->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial())
            {
                offsets[NAME("MaterialsBuffer")] = ShaderDataOffset<MaterialShaderData>(draw_call.render_material != nullptr ? draw_call.render_material->GetBufferIndex() : 0);
            }

            frame->GetCommandList().Add<BindDescriptorSet>(
                entity_descriptor_set,
                pipeline,
                offsets,
                entity_descriptor_set_index);
        }

        const SizeType offset = entity_instance_batch->batch_index * draw_call_collection.impl->GetBatchSizeOf();

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
    const DrawCallCollection& draw_call_collection,
    ParallelRenderingState* parallel_rendering_state)
{
    HYP_SCOPE;

    if constexpr (UseIndirectRendering)
    {
        AssertDebug(indirect_renderer != nullptr);
    }

    AssertDebug(parallel_rendering_state != nullptr);

    static const bool use_bindless_textures = g_render_backend->GetRenderConfig().IsBindlessSupported();

    if (draw_call_collection.instanced_draw_calls.Empty() && draw_call_collection.draw_calls.Empty())
    {
        // No draw calls to render
        return;
    }

    ValidatePipelineState(render_setup, pipeline);

    const uint32 frame_index = frame->GetFrameIndex();

    const uint32 global_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
    const DescriptorSetRef& global_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index);

    const uint32 view_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));
    const uint32 material_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Material"));

    CmdList& base_command_list = parallel_rendering_state->base_command_list;

    base_command_list.Add<BindGraphicsPipeline>(pipeline);

    if (global_descriptor_set_index != ~0u)
    {
        RenderProxyEnvProbe* env_probe_proxy = nullptr;

        if (render_setup.env_probe)
        {
            if (IRenderProxy* proxy = RenderApi_GetRenderProxy(render_setup.env_probe->Id()))
            {
                env_probe_proxy = static_cast<RenderProxyEnvProbe*>(proxy);
            }
        }

        base_command_list.Add<BindDescriptorSet>(
            global_descriptor_set,
            pipeline,
            ArrayMap<Name, uint32> {
                { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) },
                { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(render_setup.env_grid ? render_setup.env_grid->GetRenderResource().GetBufferIndex() : 0) },
                { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_setup.light ? render_setup.light->GetRenderResource().GetBufferIndex() : 0) },
                { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe ? render_setup.env_probe->GetRenderResource().GetBufferIndex() : 0) } },
            global_descriptor_set_index);
    }

    if (view_descriptor_set_index != ~0u)
    {
        AssertThrow(render_setup.pass_data != nullptr);

        base_command_list.Add<BindDescriptorSet>(
            render_setup.pass_data->descriptor_sets[frame_index],
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
    if (draw_call_collection.draw_calls.Any())
    {
        DivideDrawCalls(draw_call_collection.draw_calls.ToSpan(), parallel_rendering_state->num_batches, parallel_rendering_state->draw_calls);

        ProcRef<void(Span<const DrawCall>, uint32, uint32)> proc = parallel_rendering_state->draw_call_procs.EmplaceBack([frame_index, parallel_rendering_state, &draw_call_collection, &pipeline, indirect_renderer, material_descriptor_set_index](Span<const DrawCall> draw_calls, uint32 index, uint32)
            {
                if (!draw_calls)
                {
                    return;
                }

                CmdList& command_list = parallel_rendering_state->command_lists[index];

                const uint32 entity_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Object"));
                const DescriptorSetRef& entity_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Object"), frame_index);

                for (const DrawCall& draw_call : draw_calls)
                {
                    if (entity_descriptor_set.IsValid())
                    {
                        ArrayMap<Name, uint32> offsets;
                        offsets[NAME("SkeletonsBuffer")] = ShaderDataOffset<SkeletonShaderData>(draw_call.render_skeleton != nullptr ? draw_call.render_skeleton->GetBufferIndex() : 0);
                        offsets[NAME("CurrentObject")] = ShaderDataOffset<EntityShaderData>(draw_call.entity_id.ToIndex());

                        if (g_render_backend->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial())
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

    if (draw_call_collection.instanced_draw_calls.Any())
    {
        DivideDrawCalls(draw_call_collection.instanced_draw_calls.ToSpan(), parallel_rendering_state->num_batches, parallel_rendering_state->instanced_draw_calls);

        ProcRef<void(Span<const InstancedDrawCall>, uint32, uint32)> proc = parallel_rendering_state->instanced_draw_call_procs.EmplaceBack([frame_index, parallel_rendering_state, &draw_call_collection, &pipeline, indirect_renderer, material_descriptor_set_index](Span<const InstancedDrawCall> draw_calls, uint32 index, uint32)
            {
                if (!draw_calls)
                {
                    return;
                }

                CmdList& command_list = parallel_rendering_state->command_lists[index];

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

                        if (g_render_backend->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial())
                        {
                            offsets[NAME("MaterialsBuffer")] = ShaderDataOffset<MaterialShaderData>(draw_call.render_material != nullptr ? draw_call.render_material->GetBufferIndex() : 0);
                        }

                        command_list.Add<BindDescriptorSet>(
                            entity_descriptor_set,
                            pipeline,
                            offsets,
                            entity_descriptor_set_index);
                    }

                    const SizeType offset = entity_instance_batch->batch_index * draw_call_collection.impl->GetBatchSizeOf();

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

void RenderGroup::PerformRendering(FrameBase* frame, const RenderSetup& render_setup, const DrawCallCollection& draw_call_collection, IndirectRenderer* indirect_renderer, ParallelRenderingState* parallel_rendering_state)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);
    AssertReady();

    AssertDebugMsg(render_setup.IsValid(), "RenderSetup must be valid for rendering");
    AssertDebugMsg(render_setup.HasView(), "RenderSetup must have a valid RenderView for rendering");
    AssertDebugMsg(render_setup.pass_data != nullptr, "RenderSetup must have valid PassData for rendering!");

    auto* cache_entry = render_setup.pass_data->render_group_cache.TryGet(Id().ToIndex());
    if (!cache_entry || cache_entry->render_group.GetUnsafe() != this)
    {
        cache_entry = &*render_setup.pass_data->render_group_cache.Emplace(Id().ToIndex());

        if (cache_entry->graphics_pipeline.IsValid())
        {
            SafeRelease(std::move(cache_entry->graphics_pipeline));
        }

        *cache_entry = PassData::RenderGroupCacheEntry {
            WeakHandleFromThis(),
            CreateGraphicsPipeline(render_setup.pass_data)
        };
    }

    VulkanGraphicsPipeline* vulkan_graphics_pipeline = static_cast<VulkanGraphicsPipeline*>(cache_entry->graphics_pipeline.Get());

    // DebugLog(LogType::Debug, "PerformRendering() for RenderGroup #%u w/ with renderpass format: %u for pipeline %p\tand view renderpass format: %u for renderpass %p\n",
    //     Id().Value(),
    //     vulkan_graphics_pipeline->GetRenderPass()->GetAttachments()[0]->GetFormat(),
    //     vulkan_graphics_pipeline,
    //     render_setup.pass_data->view.GetUnsafe()->GetOutputTarget().GetFramebuffer()->GetAttachment(0)->GetFormat(),
    //     static_cast<VulkanFramebuffer*>(render_setup.pass_data->view.GetUnsafe()->GetOutputTarget().GetFramebuffer().Get())->GetRenderPass().Get());

    // AssertThrow(cache_entry->graphics_pipeline->GetFramebuffers()[0]->GetAttachment(0)->GetFormat() == render_setup.pass_data->view.GetUnsafe()->GetOutputTarget().GetFramebuffer()->GetAttachment(0)->GetFormat());

    static const bool is_indirect_rendering_enabled = g_render_backend->GetRenderConfig().IsIndirectRenderingEnabled();

    const bool use_indirect_rendering = is_indirect_rendering_enabled
        && m_flags[RenderGroupFlags::INDIRECT_RENDERING]
        && (render_setup.pass_data && render_setup.pass_data->cull_data.depth_pyramid_image_view);

    if (use_indirect_rendering)
    {
        if (m_flags & RenderGroupFlags::PARALLEL_RENDERING)
        {
            RenderAll_Parallel<true>(
                frame,
                render_setup,
                cache_entry->graphics_pipeline,
                indirect_renderer,
                draw_call_collection,
                parallel_rendering_state);
        }
        else
        {
            RenderAll<true>(
                frame,
                render_setup,
                cache_entry->graphics_pipeline,
                indirect_renderer,
                draw_call_collection);
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
                cache_entry->graphics_pipeline,
                indirect_renderer,
                draw_call_collection,
                parallel_rendering_state);
        }
        else
        {
            RenderAll<false>(
                frame,
                render_setup,
                cache_entry->graphics_pipeline,
                indirect_renderer,
                draw_call_collection);
        }
    }

#if defined(HYP_ENABLE_RENDER_STATS) && defined(HYP_ENABLE_RENDER_STATS_COUNTERS)
    EngineRenderStatsCounts counts;
    counts[ERS_RENDER_GROUPS] = 1;

    g_engine->GetRenderStatsCalculator().AddCounts(counts);
#endif
}

#pragma endregion RenderGroup

} // namespace hyperion
