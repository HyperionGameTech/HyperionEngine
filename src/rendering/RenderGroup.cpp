/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderGroup.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <scene/Entity.hpp>

#include <core/util/ForEach.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>
#include <Constants.hpp>

namespace hyperion {

using renderer::Result;
using renderer::CommandBufferType;

#pragma region Render commands

struct RENDER_COMMAND(CreateGraphicsPipeline) : renderer::RenderCommand
{
    GraphicsPipelineRef                 pipeline;
    RenderPassRef                       render_pass;
    Array<FramebufferRef>               framebuffers;
    RenderGroup::AsyncCommandBuffers    command_buffers;
    RenderableAttributeSet              attributes;

    RENDER_COMMAND(CreateGraphicsPipeline)(
        const GraphicsPipelineRef &pipeline,
        const RenderPassRef &render_pass,
        const Array<FramebufferRef> &framebuffers,
        const RenderGroup::AsyncCommandBuffers &command_buffers,
        const RenderableAttributeSet &attributes
    ) : pipeline(pipeline),
        render_pass(render_pass),
        framebuffers(framebuffers),
        command_buffers(command_buffers),
        attributes(attributes)
    {
    }

    virtual ~RENDER_COMMAND(CreateGraphicsPipeline)() override = default;

    virtual Result operator()() override
    {
#if 0
        DebugLog(LogType::Debug, "Descriptor table has %u sets:\n", pipeline->GetDescriptorTable()->GetSets().Size());

        for (const auto &set : pipeline->GetDescriptorTable()->GetSets()[0]) {
            DebugLog(LogType::Debug, "\t%s %u\n", set->GetLayout().GetName().LookupString(), pipeline->GetDescriptorTable()->GetDescriptorSetIndex(set->GetLayout().GetName()));

            for (const auto &it : set->GetLayout().GetElements()) {
                DebugLog(LogType::Debug, "\t\t%s %u %u\n", it.first.LookupString(), it.second.type, it.second.binding);
            }
        }
#endif

        pipeline->SetVertexAttributes(attributes.GetMeshAttributes().vertex_attributes);
        pipeline->SetTopology(attributes.GetMeshAttributes().topology);
        pipeline->SetCullMode(attributes.GetMaterialAttributes().cull_faces);
        pipeline->SetFillMode(attributes.GetMaterialAttributes().fill_mode);
        pipeline->SetBlendFunction(attributes.GetMaterialAttributes().blend_function);
        pipeline->SetStencilFunction(attributes.GetMaterialAttributes().stencil_function);
        pipeline->SetDepthTest(bool(attributes.GetMaterialAttributes().flags & MaterialAttributeFlags::DEPTH_TEST));
        pipeline->SetDepthWrite(bool(attributes.GetMaterialAttributes().flags & MaterialAttributeFlags::DEPTH_WRITE));
        pipeline->SetRenderPass(render_pass);
        pipeline->SetFramebuffers(framebuffers);

        for (uint i = 0; i < max_frames_in_flight; i++) {
            for (uint j = 0; j < uint(command_buffers[i].Size()); j++) {
                if (!command_buffers[i][j].IsValid()) {
                    continue;
                }

#ifdef HYP_VULKAN
                command_buffers[i][j]->GetPlatformImpl().command_pool = g_engine->GetGPUDevice()->GetGraphicsQueue().command_pools[j];
#endif

                HYPERION_BUBBLE_ERRORS(command_buffers[i][j]->Create(g_engine->GetGPUInstance()->GetDevice()));
            }
        }

        return pipeline->Create(g_engine->GetGPUDevice());
    }
};

struct RENDER_COMMAND(CreateIndirectRenderer) : renderer::RenderCommand
{
    RC<IndirectRenderer> indirect_renderer;

    RENDER_COMMAND(CreateIndirectRenderer)(RC<IndirectRenderer> indirect_renderer)
        : indirect_renderer(std::move(indirect_renderer))
    {
        AssertThrow(this->indirect_renderer != nullptr);
    }

    virtual ~RENDER_COMMAND(CreateIndirectRenderer)() override = default;

    virtual Result operator()() override
    {
        indirect_renderer->Create();

        return { };
    }
};

#pragma endregion Render commands

#pragma region RenderGroup

RenderGroup::RenderGroup(
    const ShaderRef &shader,
    const RenderableAttributeSet &renderable_attributes,
    EnumFlags<RenderGroupFlags> flags
) : BasicObject(),
    m_flags(flags),
    m_shader(shader),
    m_pipeline(MakeRenderObject<GraphicsPipeline>()),
    m_renderable_attributes(renderable_attributes)
{
    if (m_shader != nullptr) {
        m_pipeline->SetShader(m_shader);
    }
}

RenderGroup::RenderGroup(
    const ShaderRef &shader,
    const RenderableAttributeSet &renderable_attributes,
    const DescriptorTableRef &descriptor_table,
    EnumFlags<RenderGroupFlags> flags
) : BasicObject(),
    m_flags(flags),
    m_pipeline(MakeRenderObject<GraphicsPipeline>(ShaderRef::unset, descriptor_table)),
    m_shader(shader),
    m_renderable_attributes(renderable_attributes)
{
    if (m_shader != nullptr) {
        m_pipeline->SetShader(m_shader);
    }
}

RenderGroup::~RenderGroup()
{
    if (m_indirect_renderer != nullptr) {
        m_indirect_renderer->Destroy();
    }
    
    m_shader.Reset();

    m_fbos.Clear();

    if (m_command_buffers != nullptr) {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            SafeRelease(std::move((*m_command_buffers)[frame_index]));
        }

        m_command_buffers.Reset();
    }

    SafeRelease(std::move(m_pipeline));
}

void RenderGroup::RemoveFramebuffer(const FramebufferRef &framebuffer)
{
    const auto it = m_fbos.Find(framebuffer);

    if (it == m_fbos.End()) {
        return;
    }
    
    m_fbos.Erase(it);
}

void RenderGroup::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();
    
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
    {
        if (m_indirect_renderer != nullptr) {
            m_indirect_renderer->Destroy();
        }
        
        m_shader.Reset();

        SafeRelease(std::move(m_fbos));

        if (m_command_buffers != nullptr) {
            for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                SafeRelease(std::move((*m_command_buffers)[frame_index]));
            }

            m_command_buffers.Reset();
        }

        SafeRelease(std::move(m_pipeline));
    }));

    HYP_LOG(Rendering, LogLevel::INFO, "Init RenderGroup with ID: #{}\n", GetID().Value());

    if (m_flags & RenderGroupFlags::INDIRECT_RENDERING) {
        m_indirect_renderer.Reset(new IndirectRenderer());
        m_indirect_renderer->Create();
    }

    if (m_flags & RenderGroupFlags::PARALLEL_RENDERING) {
        m_command_buffers.Reset(new AsyncCommandBuffers());

        for (uint i = 0; i < max_frames_in_flight; i++) {
            for (CommandBufferRef &command_buffer : (*m_command_buffers)[i]) {
                command_buffer = MakeRenderObject<CommandBuffer>(CommandBufferType::COMMAND_BUFFER_SECONDARY);
            }
        }
    }

    AssertThrowMsg(m_fbos.Any(), "No framebuffers attached to render group");

    for (const FramebufferRef &fbo : m_fbos) {
        AssertThrow(fbo.IsValid());
        
        DeferCreate(fbo, g_engine->GetGPUDevice());
    }

    AssertThrow(m_shader.IsValid());

    RenderPassRef render_pass;

    for (auto &fbo : m_fbos) {
        if (!render_pass.IsValid()) {
            render_pass = fbo->GetRenderPass();
        }
    }

    m_pipeline->SetShader(m_shader);

    if (!m_pipeline->GetDescriptorTable().IsValid()) {
        renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

        DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);
        AssertThrow(descriptor_table != nullptr);
        DeferCreate(descriptor_table, g_engine->GetGPUDevice());

        m_pipeline->SetDescriptorTable(descriptor_table);
    }

    AssertThrow(m_pipeline->GetDescriptorTable().IsValid());

    m_pipeline.SetName(CreateNameFromDynamicString(ANSIString("GraphicsPipeline_") + m_shader->GetCompiledShader()->GetName().LookupString()));

    PUSH_RENDER_COMMAND(
        CreateGraphicsPipeline, 
        m_pipeline,
        render_pass,
        m_fbos,
        m_command_buffers != nullptr ? *m_command_buffers : AsyncCommandBuffers { },
        m_renderable_attributes
    );
        
    SetReady(true);
}

void RenderGroup::CollectDrawCalls(const Array<RenderProxy> &render_proxies)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER | ThreadName::THREAD_TASK);

    AssertReady();

    if (m_flags & RenderGroupFlags::INDIRECT_RENDERING) {
        m_indirect_renderer->GetDrawState().ResetDrawState();
    }

    m_divided_draw_calls = { };

    DrawCallCollection previous_draw_state = std::move(m_draw_state);

    for (const RenderProxy &render_proxy : render_proxies) {
#ifdef HYP_DEBUG_MODE
        AssertThrow(render_proxy.mesh.IsValid());
        AssertThrow(render_proxy.material.IsValid());
#endif

        DrawCallID draw_call_id;

        if constexpr (DrawCall::unique_per_material) {
            draw_call_id = DrawCallID(render_proxy.mesh.GetID(), render_proxy.material.GetID());
        } else {
            draw_call_id = DrawCallID(render_proxy.mesh.GetID());
        }

        BufferTicket<EntityInstanceBatch> batch_index = 0;

        if (DrawCall *draw_call = previous_draw_state.TakeDrawCall(draw_call_id)) {
            // take the batch for reuse
            if ((batch_index = draw_call->batch_index)) {
                g_engine->GetRenderData()->entity_instance_batches.ResetBatch(batch_index);
            }

            draw_call->batch_index = 0;
        }

        m_draw_state.PushDrawCall(batch_index, draw_call_id, render_proxy);
    }

    previous_draw_state.ResetDrawCalls();

    // register draw calls for indirect rendering
    if (m_flags & RenderGroupFlags::INDIRECT_RENDERING) {
        for (DrawCall &draw_call : m_draw_state.draw_calls) {
            DrawCommandData draw_command_data;
            m_indirect_renderer->GetDrawState().PushDrawCall(draw_call, draw_command_data);

            draw_call.draw_command_index = draw_command_data.draw_command_index;
        }
    }
}

void RenderGroup::PerformOcclusionCulling(Frame *frame, const CullData *cull_data)
{
    if constexpr (!use_draw_indirect) {
        return;
    }

    AssertThrow((m_flags & (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING)) == (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING));

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    AssertThrow(cull_data != nullptr);

    m_indirect_renderer->ExecuteCullShaderInBatches(
        frame,
        *cull_data
    );
}

static void GetDividedDrawCalls(
    const Array<DrawCall> &draw_calls,
    uint num_batches,
    Array<Span<const DrawCall>> &out_divided_draw_calls
)
{
    out_divided_draw_calls.Resize(num_batches);

    const uint num_draw_calls = uint(draw_calls.Size());
    const uint num_draw_calls_divided = (num_draw_calls + num_batches - 1) / num_batches;

    uint draw_call_index = 0;

    for (SizeType container_index = 0; container_index < num_async_rendering_command_buffers; container_index++) {

        const uint diff_to_next_or_end = MathUtil::Min(num_draw_calls_divided, num_draw_calls - draw_call_index);

        out_divided_draw_calls[container_index] = {
            draw_calls.Begin() + draw_call_index,
            draw_calls.Begin() + draw_call_index + diff_to_next_or_end
        };

        draw_call_index += diff_to_next_or_end;
    }
}


template <bool IsIndirect>
static HYP_FORCE_INLINE void
RenderAll(
    Frame *frame,
    const GraphicsPipelineRef &pipeline,
    IndirectRenderer *indirect_renderer,
    const DrawCallCollection &draw_state
)
{
    static const bool use_bindless_textures = g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures();

    if (draw_state.draw_calls.Empty()) {
        return;
    }

    const ID<Scene> scene_id = g_engine->GetRenderState().GetScene().id;

    const uint frame_index = frame->GetFrameIndex();

    const uint num_batches = use_parallel_rendering
        ? MathUtil::Min(uint(TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER).threads.Size()), num_async_rendering_command_buffers)
        : 1u;

    const uint global_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
    const DescriptorSetRef &global_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index);

    const uint scene_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));
    const DescriptorSetRef &scene_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index);
    
    const uint material_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Material"));
    const DescriptorSetRef &material_descriptor_set = use_bindless_textures
        ? pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Material"), frame_index)
        : DescriptorSetRef::unset;
    
    const uint entity_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Object"));
    const DescriptorSetRef &entity_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Object"), frame_index);
    
    for (const DrawCall &draw_call : draw_state.draw_calls) {
        const EntityInstanceBatch &entity_batch = g_engine->GetRenderData()->entity_instance_batches.Get(draw_call.batch_index);

        pipeline->Bind(frame->GetCommandBuffer());

        global_descriptor_set->Bind(
            frame->GetCommandBuffer(),
            pipeline,
            { },
            global_descriptor_set_index
        );

        scene_descriptor_set->Bind(
            frame->GetCommandBuffer(),
            pipeline,
            {
                { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
            },
            scene_descriptor_set_index
        );

        // Bind textures globally (bindless)
        if (material_descriptor_set_index != ~0u && use_bindless_textures) {
            material_descriptor_set->Bind(frame->GetCommandBuffer(), pipeline, material_descriptor_set_index);
        }

        if (entity_descriptor_set.IsValid()) {
#ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
            entity_descriptor_set->Bind(
                frame->GetCommandBuffer(),
                pipeline,
                {
                    { NAME("SkeletonsBuffer"), HYP_RENDER_OBJECT_OFFSET(Skeleton, draw_call.skeleton_id.ToIndex()) },
                    { NAME("EntityInstanceBatchesBuffer"), uint32(draw_call.batch_index * sizeof(EntityInstanceBatch)) }
                },
                entity_descriptor_set_index
            );
#else
            entity_descriptor_set->Bind(
                frame->GetCommandBuffer(),
                pipeline,
                {
                    { NAME("MaterialsBuffer"), HYP_RENDER_OBJECT_OFFSET(Material, draw_call.material_id.ToIndex()) },
                    { NAME("SkeletonsBuffer"), HYP_RENDER_OBJECT_OFFSET(Skeleton, draw_call.skeleton_id.ToIndex()) },
                    { NAME("EntityInstanceBatchesBuffer"), uint32(draw_call.batch_index * sizeof(EntityInstanceBatch)) }
                },
                entity_descriptor_set_index
            );
#endif
        }

        // Bind material descriptor set
        if (material_descriptor_set_index != ~0u && !use_bindless_textures) {
            const DescriptorSetRef &material_descriptor_set = g_engine->GetMaterialDescriptorSetManager().GetDescriptorSet(draw_call.material_id, frame_index);
            AssertThrow(material_descriptor_set.IsValid());

            material_descriptor_set->Bind(frame->GetCommandBuffer(), pipeline, material_descriptor_set_index);
        }

        if constexpr (IsIndirect) {
            Handle<Mesh>::GetContainer().Get(draw_call.mesh_id.ToIndex())
                .RenderIndirect(
                    frame->GetCommandBuffer(),
                    indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index).Get(),
                    draw_call.draw_command_index * uint32(sizeof(IndirectDrawCommand))
                );
        } else {
            Handle<Mesh>::GetContainer().Get(draw_call.mesh_id.ToIndex())
                .Render(frame->GetCommandBuffer(), entity_batch.num_entities);
        }
    }
}

template <bool IsIndirect>
static HYP_FORCE_INLINE void
RenderAll_Parallel(
    Frame *frame,
    const RenderGroup::AsyncCommandBuffers &command_buffers,
    uint &command_buffer_index,
    const GraphicsPipelineRef &pipeline,
    IndirectRenderer *indirect_renderer,
    Array<Span<const DrawCall>> &divided_draw_calls,
    const DrawCallCollection &draw_state
)
{
    static const bool use_bindless_textures = g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures();

    if (draw_state.draw_calls.Empty()) {
        return;
    }

    const ID<Scene> scene_id = g_engine->GetRenderState().GetScene().id;

    const uint frame_index = frame->GetFrameIndex();

    const uint num_batches = MathUtil::Min(uint(TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER).threads.Size()), num_async_rendering_command_buffers);
    
    GetDividedDrawCalls(
        draw_state.draw_calls,
        num_async_rendering_command_buffers,
        divided_draw_calls
    );

    // rather than using a single integer, we have to set states in a fixed array
    // because otherwise we'd need to use an atomic integer
    FixedArray<uint, num_async_rendering_command_buffers> command_buffers_recorded_states { };

    const uint global_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
    const DescriptorSetRef &global_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index);

    const uint scene_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));
    const DescriptorSetRef &scene_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index);
    
    const uint material_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Material"));
    const DescriptorSetRef &material_descriptor_set = use_bindless_textures
        ? pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Material"), frame_index)
        : DescriptorSetRef::unset;
    
    const uint entity_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Object"));
    const DescriptorSetRef &entity_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Object"), frame_index);

    // AtomicVar<uint32> num_rendered_objects { 0u };

    if (divided_draw_calls.Size() == 1) {
        RenderAll<IsIndirect>(
            frame,
            pipeline,
            indirect_renderer,
            draw_state
        );

        return;
    }

    // HYP_LOG(Rendering, LogLevel::DEBUG, "Rendering {} draw calls in {} batches", draw_state.draw_calls.Size(), num_batches);

    ParallelForEach(divided_draw_calls, num_batches, TaskThreadPoolName::THREAD_POOL_RENDER,
        [&](Span<const DrawCall> draw_calls, uint index, uint)
        {
            if (!draw_calls) {
                return;
            }

            auto &mesh_container = Handle<Mesh>::GetContainer();

            command_buffers[frame_index][index]->Record(
                g_engine->GetGPUDevice(),
                pipeline->GetRenderPass(),
                [&](CommandBuffer *secondary)
                {
                    pipeline->Bind(secondary);
                    
                    global_descriptor_set->Bind(
                        secondary,
                        pipeline,
                        { },
                        global_descriptor_set_index
                    );

                    scene_descriptor_set->Bind(
                        secondary,
                        pipeline,
                        {
                            { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                            { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                            { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                            { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                            { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                        },
                        scene_descriptor_set_index
                    );

                    // Bind textures globally (bindless)
                    if (material_descriptor_set_index != ~0u && use_bindless_textures) {
                        material_descriptor_set->Bind(secondary, pipeline, material_descriptor_set_index);
                    }

                    for (const DrawCall &draw_call : draw_calls) {
                        const EntityInstanceBatch &entity_batch = g_engine->GetRenderData()->entity_instance_batches.Get(draw_call.batch_index);
                        
                        if (entity_descriptor_set.IsValid()) {
#ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
                            entity_descriptor_set->Bind(
                                secondary,
                                pipeline,
                                {
                                    { NAME("SkeletonsBuffer"), HYP_RENDER_OBJECT_OFFSET(Skeleton, draw_call.skeleton_id.ToIndex()) },
                                    { NAME("EntityInstanceBatchesBuffer"), uint32(draw_call.batch_index * sizeof(EntityInstanceBatch)) }
                                },
                                entity_descriptor_set_index
                            );
#else
                            entity_descriptor_set->Bind(
                                secondary,
                                pipeline,
                                {
                                    { NAME("MaterialsBuffer"), HYP_RENDER_OBJECT_OFFSET(Material, draw_call.material_id.ToIndex()) },
                                    { NAME("SkeletonsBuffer"), HYP_RENDER_OBJECT_OFFSET(Skeleton, draw_call.skeleton_id.ToIndex()) },
                                    { NAME("EntityInstanceBatchesBuffer"), uint32(draw_call.batch_index * sizeof(EntityInstanceBatch)) }
                                },
                                entity_descriptor_set_index
                            );
#endif
                        }

                        // Bind material descriptor set
                        if (material_descriptor_set_index != ~0u && !use_bindless_textures) {
                            const DescriptorSetRef &material_descriptor_set = g_engine->GetMaterialDescriptorSetManager().GetDescriptorSet(draw_call.material_id, frame_index);
                            AssertThrow(material_descriptor_set.IsValid());

                            material_descriptor_set->Bind(secondary, pipeline, material_descriptor_set_index);
                        }

                        if constexpr (IsIndirect) {
                            mesh_container.Get(draw_call.mesh_id.ToIndex())
                                .RenderIndirect(
                                    secondary,
                                    indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index).Get(),
                                    draw_call.draw_command_index * uint32(sizeof(IndirectDrawCommand))
                                );
                        } else {
                            mesh_container.Get(draw_call.mesh_id.ToIndex())
                                .Render(secondary, entity_batch.num_entities);
                        }
                    }

                    HYPERION_RETURN_OK;
                }
            );

            command_buffers_recorded_states[index] = 1u;
        }
    );
    

    const uint num_recorded_command_buffers = command_buffers_recorded_states.Sum();

    // submit all command buffers
    for (uint i = 0; i < num_recorded_command_buffers; i++) {
        command_buffers[frame_index][i]
            ->SubmitSecondary(frame->GetCommandBuffer());
    }

    command_buffer_index = (command_buffer_index + num_recorded_command_buffers) % uint(command_buffers.Size());
}

void RenderGroup::PerformRendering(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    AssertReady();

    if (m_draw_state.draw_calls.Empty()) {
        return;
    }

    if (m_flags & RenderGroupFlags::PARALLEL_RENDERING) {
        AssertThrow(m_command_buffers != nullptr);

        RenderAll_Parallel<false>(
            frame,
            *m_command_buffers,
            m_command_buffer_index,
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

void RenderGroup::PerformRenderingIndirect(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    AssertReady();

    AssertThrow(m_flags & RenderGroupFlags::INDIRECT_RENDERING);

    if (m_draw_state.draw_calls.Empty()) {
        return;
    }

    if (m_flags & RenderGroupFlags::PARALLEL_RENDERING) {
        AssertThrow(m_command_buffers != nullptr);

        RenderAll_Parallel<true>(
            frame,
            *m_command_buffers,
            m_command_buffer_index,
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

const CommandBufferRef &RendererProxy::GetCommandBuffer(uint frame_index) const
{
    AssertThrowMsg(m_render_group->m_command_buffers != nullptr, "Must have PARALLEL_RENDERING flag set");

    return (*m_render_group->m_command_buffers)[frame_index].Front();
}

const GraphicsPipelineRef &RendererProxy::GetGraphicsPipeline() const
{
    return m_render_group->m_pipeline;
}

void RendererProxy::Bind(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    CommandBuffer *command_buffer = GetCommandBuffer(frame->GetFrameIndex());
    AssertThrow(command_buffer != nullptr);

    command_buffer->Begin(g_engine->GetGPUDevice(), m_render_group->m_pipeline->GetRenderPass());

    m_render_group->m_pipeline->Bind(command_buffer);
}

void RendererProxy::DrawMesh(Frame *frame, Mesh *mesh)
{
    CommandBuffer *command_buffer = GetCommandBuffer(frame->GetFrameIndex());
    AssertThrow(command_buffer != nullptr);

    mesh->Render(command_buffer);
}

void RendererProxy::Submit(Frame *frame)
{
    CommandBuffer *command_buffer = GetCommandBuffer(frame->GetFrameIndex());
    AssertThrow(command_buffer != nullptr);

    command_buffer->End(g_engine->GetGPUDevice());
    command_buffer->SubmitSecondary(frame->GetCommandBuffer());
}

#pragma endregion RendererProxy

} // namespace hyperion
