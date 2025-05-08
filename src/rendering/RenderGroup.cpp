/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderGroup.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderSkeleton.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/EnvGrid.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/RenderState.hpp>
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
      m_draw_state(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>(max_entity_instance_batches), this)
{
}

RenderGroup::RenderGroup(
    const ShaderRef &shader,
    const RenderableAttributeSet &renderable_attributes,
    EnumFlags<RenderGroupFlags> flags
) : HypObject(),
    m_flags(flags),
    m_shader(shader),
    m_renderable_attributes(renderable_attributes),
    m_draw_state(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>(max_entity_instance_batches), this)
{
}

RenderGroup::RenderGroup(
    const ShaderRef &shader,
    const RenderableAttributeSet &renderable_attributes,
    const DescriptorTableRef &descriptor_table,
    EnumFlags<RenderGroupFlags> flags
) : HypObject(),
    m_flags(flags),
    m_shader(shader),
    m_descriptor_table(descriptor_table),
    m_renderable_attributes(renderable_attributes),
    m_draw_state(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>(max_entity_instance_batches), this)
{
}

RenderGroup::~RenderGroup()
{
    // for (auto &it : m_render_proxies) {
    //     it.second->UnclaimRenderResource();
    // }

    if (m_indirect_renderer != nullptr) {
        m_indirect_renderer->Destroy();
    }
    
    SafeRelease(std::move(m_shader));
    SafeRelease(std::move(m_descriptor_table));
    SafeRelease(std::move(m_fbos));
    SafeRelease(std::move(m_pipeline));
}

void RenderGroup::SetShader(const ShaderRef &shader)
{
    HYP_SCOPE;
    
    SafeRelease(std::move(m_shader));

    m_shader = shader;

    // @TODO Re-create pipeline
}

void RenderGroup::SetRenderableAttributes(const RenderableAttributeSet &renderable_attributes)
{
    m_renderable_attributes = renderable_attributes;
}

void RenderGroup::AddFramebuffer(const FramebufferRef &framebuffer)
{
    HYP_SCOPE;
    
    const auto it = m_fbos.Find(framebuffer);

    if (it != m_fbos.End()) {
        return;
    }

    m_fbos.PushBack(framebuffer);

    // @TODO Re-create pipeline
}

void RenderGroup::RemoveFramebuffer(const FramebufferRef &framebuffer)
{
    HYP_SCOPE;
    
    const auto it = m_fbos.Find(framebuffer);

    if (it == m_fbos.End()) {
        return;
    }
    
    m_fbos.Erase(it);
}

void RenderGroup::Init()
{
    HYP_SCOPE;
    
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
    {
        HYP_SCOPE;

        if (m_indirect_renderer != nullptr) {
            m_indirect_renderer->Destroy();
        }
        
        SafeRelease(std::move(m_shader));
        SafeRelease(std::move(m_descriptor_table));
        SafeRelease(std::move(m_fbos));
        SafeRelease(std::move(m_pipeline));
    }));

    // If parallel rendering is globally disabled, disable it for this RenderGroup
    if (!g_rendering_api->GetRenderConfig().IsParallelRenderingEnabled()) {
        m_flags &= ~RenderGroupFlags::PARALLEL_RENDERING;
    }

    if (!g_rendering_api->GetRenderConfig().IsIndirectRenderingEnabled()) {
        m_flags &= ~RenderGroupFlags::INDIRECT_RENDERING;
    }

    CreateIndirectRenderer();
    CreateGraphicsPipeline();
        
    SetReady(true);
}

void RenderGroup::CreateIndirectRenderer()
{
    HYP_SCOPE;
    
    if (m_flags & RenderGroupFlags::INDIRECT_RENDERING) {
        m_indirect_renderer = MakeUnique<IndirectRenderer>(&m_draw_state);
        m_indirect_renderer->Create();
    }
}

void RenderGroup::CreateGraphicsPipeline()
{
    HYP_SCOPE;

    AssertThrowMsg(m_fbos.Any(), "No framebuffers attached to RenderGroup");

    for (const FramebufferRef &framebuffer : m_fbos) {
        AssertThrow(framebuffer.IsValid());
        
        DeferCreate(framebuffer);
    }

    AssertThrow(m_shader.IsValid());

    if (!m_descriptor_table.IsValid()) {
        renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

        m_descriptor_table = g_rendering_api->MakeDescriptorTable(descriptor_table_decl);
        m_descriptor_table->SetDebugName(NAME_FMT("DescriptorTable_{}", m_shader->GetCompiledShader()->GetName()));

        // Setup instancing buffers if "Instancing" descriptor set exists
        const uint32 instancing_descriptor_set_index = m_descriptor_table->GetDescriptorSetIndex(NAME("Instancing"));

        if (instancing_descriptor_set_index != ~0u) {
            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                const GPUBufferRef &gpu_buffer = m_draw_state.GetImpl()->GetEntityInstanceBatchHolder()->GetBuffer(frame_index);
                AssertThrow(gpu_buffer.IsValid());

                const DescriptorSetRef &instancing_descriptor_set = m_descriptor_table->GetDescriptorSet(NAME("Instancing"), frame_index);
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
        m_renderable_attributes
    );
}

void RenderGroup::ClearProxies()
{
    Threads::AssertOnThread(g_render_thread);

    // for (auto &it : m_render_proxies) {
    //     it.second->UnclaimRenderResource();
    // }

    m_render_proxies.Clear();
}

void RenderGroup::AddRenderProxy(const RenderProxy &render_proxy)
{
    Threads::AssertOnThread(g_render_thread);

    AssertDebug(render_proxy.mesh.IsValid());
    AssertDebug(render_proxy.mesh->IsReady());

    AssertDebug(render_proxy.material.IsValid());
    AssertDebug(render_proxy.material->IsReady());

    AssertThrow(m_render_proxies.Insert(render_proxy.entity.GetID(), &render_proxy).second);

    // render_proxy.ClaimRenderResource();
}

bool RenderGroup::RemoveRenderProxy(ID<Entity> entity)
{
    Threads::AssertOnThread(g_render_thread);

    const auto it = m_render_proxies.Find(entity);

    if (it == m_render_proxies.End()) {
        return false;
    }

    // it->second->UnclaimRenderResource();

    m_render_proxies.Erase(it);

    return true;
}

typename FlatMap<ID<Entity>, const RenderProxy *>::Iterator RenderGroup::RemoveRenderProxy(typename FlatMap<ID<Entity>, const RenderProxy *>::ConstIterator iterator)
{
    Threads::AssertOnThread(g_render_thread);

    return m_render_proxies.Erase(iterator);
}

void RenderGroup::SetDrawCallCollectionImpl(IDrawCallCollectionImpl *draw_call_collection_impl)
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

    if (m_flags & RenderGroupFlags::INDIRECT_RENDERING) {
        m_indirect_renderer->GetDrawState().ResetDrawState();
    }

    m_divided_draw_calls = { };

    DrawCallCollection previous_draw_state = std::move(m_draw_state);

    for (const auto &it : m_render_proxies) {
        const RenderProxy *render_proxy = it.second;

        if (render_proxy->instance_data.num_instances == 0) {
            continue;
        }

        AssertDebug(render_proxy->mesh.IsValid());
        AssertDebugMsg(render_proxy->mesh->IsReady(), "Mesh #%u is not ready", render_proxy->mesh.GetID().Value());

        AssertDebug(render_proxy->material.IsValid());
        AssertDebugMsg(render_proxy->material->IsReady(), "Material #%u is not ready", render_proxy->material.GetID().Value());

        DrawCallID draw_call_id;

        if (unique_per_material) {
            // @TODO: Rather than using Material ID we could use hashcode of the material,
            // so that we can use the same material with different IDs
            draw_call_id = DrawCallID(render_proxy->mesh.GetID(), render_proxy->material.GetID());
        } else {
            draw_call_id = DrawCallID(render_proxy->mesh.GetID());
        }

        EntityInstanceBatch *batch = nullptr;

        // take a batch for reuse if a draw call was using one
        if ((batch = previous_draw_state.TakeDrawCallBatch(draw_call_id)) != nullptr) {
            const uint32 batch_index = batch->batch_index;
            AssertDebug(batch_index != ~0u);

            // Reset it
            *batch = EntityInstanceBatch { batch_index };

            m_draw_state.GetImpl()->GetEntityInstanceBatchHolder()->MarkDirty(batch->batch_index);
        }

        m_draw_state.PushDrawCallToBatch(batch, draw_call_id, *render_proxy);
    }

    previous_draw_state.ResetDrawCalls();

    // register draw calls for indirect rendering
    if (m_flags & RenderGroupFlags::INDIRECT_RENDERING) {
        m_indirect_renderer->PushDrawCallsToIndirectState();
    }
}

void RenderGroup::PerformOcclusionCulling(FrameBase *frame, const CullData *cull_data)
{
    HYP_SCOPE;
    
    static const bool is_indirect_rendering_enabled = g_rendering_api->GetRenderConfig().IsIndirectRenderingEnabled();

    if (!is_indirect_rendering_enabled) {
        return;
    }

    AssertThrow((m_flags & (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING)) == (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING));

    Threads::AssertOnThread(g_render_thread);

    AssertThrow(cull_data != nullptr);

    m_indirect_renderer->ExecuteCullShaderInBatches(
        frame,
        *cull_data
    );
}

static void GetDividedDrawCalls(Span<const DrawCall> draw_calls, uint32 num_batches, Array<Span<const DrawCall>> &out_divided_draw_calls)
{
    HYP_SCOPE;

    const uint32 num_draw_calls = uint32(draw_calls.Size());

    // Make sure we don't try to divide into more batches than we have draw calls
    num_batches = MathUtil::Min(num_batches, num_draw_calls);
    out_divided_draw_calls.Resize(num_batches);

    const uint32 num_draw_calls_divided = (num_draw_calls + num_batches - 1) / num_batches;

    uint32 draw_call_index = 0;

    for (uint32 container_index = 0; container_index < num_batches; container_index++) {
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


template <bool IsIndirect>
static void RenderAll(
    FrameBase *frame,
    const GraphicsPipelineRef &pipeline,
    IndirectRenderer *indirect_renderer,
    const DrawCallCollection &draw_state
)
{
    HYP_SCOPE;

    static const bool use_bindless_textures = g_rendering_api->GetRenderConfig().IsBindlessSupported();

    if (draw_state.GetDrawCalls().Empty()) {
        return;
    }

    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();
    const TResourceHandle<CameraRenderResource> &camera_resource_handle = g_engine->GetRenderState()->GetActiveCamera();
    const TResourceHandle<LightRenderResource> &light_resource_handle = g_engine->GetRenderState()->GetActiveLight();
    const TResourceHandle<EnvProbeRenderResource> &env_probe_resource_handle = g_engine->GetRenderState()->GetActiveEnvProbe();
    EnvGrid *env_grid = g_engine->GetRenderState()->GetActiveEnvGrid();

    const uint32 frame_index = frame->GetFrameIndex();

    const uint32 global_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
    const DescriptorSetRef &global_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index);

    const uint32 scene_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));
    const DescriptorSetRef &scene_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index);
    
    const uint32 material_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Material"));
    const DescriptorSetRef &material_descriptor_set = use_bindless_textures
        ? pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Material"), frame_index)
        : DescriptorSetRef::unset;
    
    const uint32 entity_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Object"));
    const DescriptorSetRef &entity_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Object"), frame_index);

    const uint32 instancing_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Instancing"));
    const DescriptorSetRef &instancing_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Instancing"), frame_index);

    EngineRenderStatsCounts counts;
    
    for (const DrawCall &draw_call : draw_state.GetDrawCalls()) {
        EntityInstanceBatch *batch = draw_call.batch;
        AssertDebug(batch != nullptr);

        frame->GetCommandList().Add<BindGraphicsPipeline>(pipeline);

        frame->GetCommandList().Add<BindDescriptorSet>(
            global_descriptor_set,
            pipeline,
            ArrayMap<Name, uint32> { },
            global_descriptor_set_index
        );

        frame->GetCommandList().Add<BindDescriptorSet>(
            scene_descriptor_set,
            pipeline,
            ArrayMap<Name, uint32> {
                { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*camera_resource_handle) },
                { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid ? env_grid->GetComponentIndex() : 0) },
                { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(light_resource_handle.Get(), 0) },
                { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_resource_handle.Get(), 0) }
            },
            scene_descriptor_set_index
        );

        // Bind textures globally (bindless)
        if (material_descriptor_set_index != ~0u && use_bindless_textures) {
            frame->GetCommandList().Add<BindDescriptorSet>(
                material_descriptor_set,
                pipeline,
                ArrayMap<Name, uint32> { },
                material_descriptor_set_index
            );
        }

        if (entity_descriptor_set.IsValid()) {
            if (g_rendering_api->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial()) {
                frame->GetCommandList().Add<BindDescriptorSet>(
                    entity_descriptor_set,
                    pipeline,
                    ArrayMap<Name, uint32> {
                        { NAME("MaterialsBuffer"), ShaderDataOffset<MaterialShaderData>(draw_call.material_render_resource != nullptr ? draw_call.material_render_resource->GetBufferIndex() : 0) },
                        { NAME("SkeletonsBuffer"), ShaderDataOffset<SkeletonShaderData>(draw_call.skeleton_render_resource != nullptr ? draw_call.skeleton_render_resource->GetBufferIndex() : 0) }
                    },
                    entity_descriptor_set_index
                );
            } else {
                frame->GetCommandList().Add<BindDescriptorSet>(
                    entity_descriptor_set,
                    pipeline,
                    ArrayMap<Name, uint32> {
                        { NAME("SkeletonsBuffer"), ShaderDataOffset<SkeletonShaderData>(draw_call.skeleton_render_resource != nullptr ? draw_call.skeleton_render_resource->GetBufferIndex() : 0) }
                    },
                    entity_descriptor_set_index
                );
            }
        }

        if (instancing_descriptor_set.IsValid()) {
            frame->GetCommandList().Add<BindDescriptorSet>(
                instancing_descriptor_set,
                pipeline,
                ArrayMap<Name, uint32> {
                    { NAME("EntityInstanceBatchesBuffer"), uint32(batch->batch_index * draw_state.GetImpl()->GetBatchSizeOf()) }
                },
                instancing_descriptor_set_index
            );
        }

        // Bind material descriptor set
        if (material_descriptor_set_index != ~0u && !use_bindless_textures) {
            const DescriptorSetRef &material_descriptor_set = draw_call.material_render_resource->GetDescriptorSets()[frame_index];
            AssertThrow(material_descriptor_set.IsValid());

            frame->GetCommandList().Add<BindDescriptorSet>(
                material_descriptor_set,
                pipeline,
                ArrayMap<Name, uint32> { },
                material_descriptor_set_index
            );
        }

        if constexpr (IsIndirect) {
            draw_call.mesh_render_resource->RenderIndirect(
                frame->GetCommandList(),
                indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index),
                draw_call.draw_command_index * uint32(sizeof(IndirectDrawCommand))
            );
        } else {
            draw_call.mesh_render_resource->Render(frame->GetCommandList(), batch->num_entities);
        }

        counts.num_draw_calls++;
        counts.num_triangles += draw_call.mesh_render_resource->NumIndices() / 3;
    }

    g_engine->GetRenderStatsCalculator().AddCounts(counts);
}

template <bool IsIndirect>
static void RenderAll_Parallel(
    FrameBase *frame,
    const GraphicsPipelineRef &pipeline,
    IndirectRenderer *indirect_renderer,
    Array<Span<const DrawCall>> &divided_draw_calls,
    const DrawCallCollection &draw_state
)
{
    HYP_SCOPE;

    static const bool use_bindless_textures = g_rendering_api->GetRenderConfig().IsBindlessSupported();

    if (draw_state.GetDrawCalls().Empty()) {
        return;
    }

    const uint32 frame_index = frame->GetFrameIndex();

    TaskThreadPool &pool = TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER);

    const uint32 num_batches = MathUtil::Min(pool.NumThreads(), num_async_rendering_command_buffers);
    
    GetDividedDrawCalls(draw_state.GetDrawCalls().ToSpan(), num_async_rendering_command_buffers, divided_draw_calls);

    const uint32 global_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
    const DescriptorSetRef &global_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index);

    const uint32 scene_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));
    const DescriptorSetRef &scene_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index);
    
    const uint32 material_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Material"));
    const DescriptorSetRef &material_descriptor_set = use_bindless_textures
        ? pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Material"), frame_index)
        : DescriptorSetRef::unset;
    
    const uint32 entity_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Object"));
    const DescriptorSetRef &entity_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Object"), frame_index);

    const uint32 instancing_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Instancing"));
    const DescriptorSetRef &instancing_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Instancing"), frame_index);

    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();
    const TResourceHandle<CameraRenderResource> &camera_resource_handle = g_engine->GetRenderState()->GetActiveCamera();
    const TResourceHandle<LightRenderResource> &light_resource_handle = g_engine->GetRenderState()->GetActiveLight();
    const TResourceHandle<EnvProbeRenderResource> &env_probe_resource_handle = g_engine->GetRenderState()->GetActiveEnvProbe();
    EnvGrid *env_grid = g_engine->GetRenderState()->GetActiveEnvGrid();

    // HYP_LOG(Rendering, Debug, "Rendering {} draw calls in {} batches", draw_state.GetDrawCalls().Size(), num_batches);

    // rather than using a single integer, we have to set states in a fixed array
    // because otherwise we'd need to use an atomic integer
    FixedArray<RHICommandList, num_async_rendering_command_buffers> command_lists { };
    FixedArray<uint32, num_async_rendering_command_buffers> record_states { };

    FixedArray<EngineRenderStatsCounts, num_async_rendering_command_buffers> render_stats_counts { };

    TaskSystem::GetInstance().ParallelForEach(
        HYP_STATIC_MESSAGE("RenderAll_Parallel"),
        pool,
        num_batches,
        divided_draw_calls,
        [&](Span<const DrawCall> draw_calls, uint32 index, uint32)
        {
            if (!draw_calls) {
                return;
            }

            RHICommandList &command_list = command_lists[index];
            
            command_list.Add<BindGraphicsPipeline>(pipeline);

            command_list.Add<BindDescriptorSet>(
                global_descriptor_set,
                pipeline,
                ArrayMap<Name, uint32> { },
                global_descriptor_set_index
            );

            command_list.Add<BindDescriptorSet>(
                scene_descriptor_set,
                pipeline,
                ArrayMap<Name, uint32> {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*camera_resource_handle) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid ? env_grid->GetComponentIndex() : 0) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(light_resource_handle.Get(), 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_resource_handle.Get(), 0) }
                },
                scene_descriptor_set_index
            );

            // Bind textures globally (bindless)
            if (material_descriptor_set_index != ~0u && use_bindless_textures) {
                command_list.Add<BindDescriptorSet>(
                    material_descriptor_set,
                    pipeline,
                    ArrayMap<Name, uint32> { },
                    material_descriptor_set_index
                );
            }

            for (const DrawCall &draw_call : draw_calls) {
                EntityInstanceBatch *entity_instance_batch = draw_call.batch;
                AssertDebug(entity_instance_batch != nullptr);

                if (entity_descriptor_set.IsValid()) {
                    static const bool unique_per_material = g_rendering_api->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial();

                    if (unique_per_material) {
                        command_list.Add<BindDescriptorSet>(
                            entity_descriptor_set,
                            pipeline,
                            ArrayMap<Name, uint32> {
                                { NAME("MaterialsBuffer"), ShaderDataOffset<MaterialShaderData>(draw_call.material_render_resource != nullptr ? draw_call.material_render_resource->GetBufferIndex() : 0) },
                                { NAME("SkeletonsBuffer"), ShaderDataOffset<SkeletonShaderData>(draw_call.skeleton_render_resource != nullptr ? draw_call.skeleton_render_resource->GetBufferIndex() : 0) }
                            },
                            entity_descriptor_set_index
                        );
                    } else {
                        command_list.Add<BindDescriptorSet>(
                            entity_descriptor_set,
                            pipeline,
                            ArrayMap<Name, uint32> {
                                { NAME("SkeletonsBuffer"), ShaderDataOffset<SkeletonShaderData>(draw_call.skeleton_render_resource != nullptr ? draw_call.skeleton_render_resource->GetBufferIndex() : 0) }
                            },
                            entity_descriptor_set_index
                        );
                    }
                }

                if (instancing_descriptor_set.IsValid()) {
                    command_list.Add<BindDescriptorSet>(
                        instancing_descriptor_set,
                        pipeline,
                        ArrayMap<Name, uint32> {
                            { NAME("EntityInstanceBatchesBuffer"), uint32(entity_instance_batch->batch_index * draw_state.GetImpl()->GetBatchSizeOf()) }
                        },
                        instancing_descriptor_set_index
                    );
                }

                // Bind material descriptor set
                if (material_descriptor_set_index != ~0u && !use_bindless_textures) {
                    const DescriptorSetRef &material_descriptor_set = draw_call.material_render_resource->GetDescriptorSets()[frame_index];
                    AssertThrow(material_descriptor_set.IsValid());

                    command_list.Add<BindDescriptorSet>(
                        material_descriptor_set,
                        pipeline,
                        ArrayMap<Name, uint32> { },
                        material_descriptor_set_index
                    );
                }

                if constexpr (IsIndirect) {
                    draw_call.mesh_render_resource->RenderIndirect(
                        command_list,
                        indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index),
                        draw_call.draw_command_index * uint32(sizeof(IndirectDrawCommand))
                    );
                } else {
                    draw_call.mesh_render_resource->Render(command_list, entity_instance_batch->num_entities);
                }

                render_stats_counts[index].num_triangles += draw_call.mesh_render_resource->NumIndices() / 3;
                render_stats_counts[index].num_draw_calls++;
            }


            record_states[index] = 1u;
        }
    );

    const uint32 num_recorded_command_lists = record_states.Sum();

    // submit all command buffers
    for (uint32 i = 0; i < num_recorded_command_lists; i++) {
        frame->GetCommandList().Concat(std::move(command_lists[i]));
    }

    for (const EngineRenderStatsCounts &counts : render_stats_counts) {
        g_engine->GetRenderStatsCalculator().AddCounts(counts);
    }
}

void RenderGroup::PerformRendering(FrameBase *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);
    AssertReady();

    if (m_draw_state.GetDrawCalls().Empty()) {
        return;
    }

    if (m_flags & RenderGroupFlags::PARALLEL_RENDERING) {
        RenderAll_Parallel<false>(
            frame,
            m_pipeline,
            m_indirect_renderer.Get(),
            m_divided_draw_calls,
            m_draw_state
        );
    } else {
        RenderAll<false>(
            frame,
            m_pipeline,
            m_indirect_renderer.Get(),
            m_draw_state
        );
    }
}

void RenderGroup::PerformRenderingIndirect(FrameBase *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);
    AssertReady();

    AssertThrow(m_flags & RenderGroupFlags::INDIRECT_RENDERING);

    if (m_draw_state.GetDrawCalls().Empty()) {
        return;
    }

    if (m_flags & RenderGroupFlags::PARALLEL_RENDERING) {
        RenderAll_Parallel<true>(
            frame,
            m_pipeline,
            m_indirect_renderer.Get(),
            m_divided_draw_calls,
            m_draw_state
        );
    } else {
        RenderAll<true>(
            frame,
            m_pipeline,
            m_indirect_renderer.Get(),
            m_draw_state
        );
    }
}

#pragma endregion RenderGroup

#pragma region RendererProxy


#pragma endregion RendererProxy

} // namespace hyperion
