/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rendering/RenderGroup.hpp>
#include <scene/Entity.hpp>
#include <Engine.hpp>
#include <Constants.hpp>

#include <core/lib/util/ForEach.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion::v2 {

using renderer::Result;
using renderer::CommandBufferType;

#pragma region Render commands

struct RENDER_COMMAND(CreateGraphicsPipeline) : renderer::RenderCommand
{
    GraphicsPipelineRef                 pipeline;
    RenderPassRef                       render_pass;
    Array<FramebufferObjectRef>         framebuffers;
    RenderGroup::AsyncCommandBuffers    command_buffers;
    RenderableAttributeSet              attributes;

    RENDER_COMMAND(CreateGraphicsPipeline)(
        GraphicsPipelineRef pipeline,
        RenderPassRef render_pass,
        Array<FramebufferObjectRef> framebuffers,
        RenderGroup::AsyncCommandBuffers command_buffers,
        const RenderableAttributeSet &attributes
    ) : pipeline(std::move(pipeline)),
        render_pass(std::move(render_pass)),
        framebuffers(std::move(framebuffers)),
        command_buffers(std::move(command_buffers)),
        attributes(attributes)
    {
    }

    virtual ~RENDER_COMMAND(CreateGraphicsPipeline)() override = default;

    virtual Result operator()() override
    {
        DebugLog(LogType::Debug, "Create GraphicsPipeline %s\n", pipeline.GetName().LookupString());

        AssertThrow(pipeline->GetDescriptorTable().IsValid());

        DebugLog(LogType::Debug, "Descriptor table has %u sets:\n", pipeline->GetDescriptorTable()->GetSets().Size());

        for (const auto &set : pipeline->GetDescriptorTable()->GetSets()[0]) {
            DebugLog(LogType::Debug, "\t%s %u\n", set->GetLayout().GetName().LookupString(), pipeline->GetDescriptorTable()->GetDescriptorSetIndex(set->GetLayout().GetName()));

            for (const auto &it : set->GetLayout().GetElements()) {
                DebugLog(LogType::Debug, "\t\t%s %u %u\n", it.first.LookupString(), it.second.type, it.second.binding);
            }
        }

        renderer::GraphicsPipeline::ConstructionInfo construction_info {
            .vertex_attributes = attributes.GetMeshAttributes().vertex_attributes,
            .topology          = attributes.GetMeshAttributes().topology,
            .cull_mode         = attributes.GetMaterialAttributes().cull_faces,
            .fill_mode         = attributes.GetMaterialAttributes().fill_mode,
            .blend_function    = attributes.GetMaterialAttributes().blend_function,
            .depth_test        = bool(attributes.GetMaterialAttributes().flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST),
            .depth_write       = bool(attributes.GetMaterialAttributes().flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE),
            .render_pass       = render_pass,
            .stencil_state     = attributes.GetStencilState()
        };

        for (FramebufferObjectRef &framebuffer : framebuffers) {
            construction_info.fbos.PushBack(std::move(framebuffer));
        }

        for (uint i = 0; i < max_frames_in_flight; i++) {
            for (uint j = 0; j < uint(command_buffers[i].Size()); j++) {
                HYPERION_BUBBLE_ERRORS(command_buffers[i][j]->Create(
                    g_engine->GetGPUInstance()->GetDevice(),
                    g_engine->GetGPUDevice()->GetGraphicsQueue().command_pools[j]
                ));
            }
        }

        return pipeline->Create(
            g_engine->GetGPUDevice(),
            std::move(construction_info)
        );
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

        return renderer::Result { };
    }
};

#pragma endregion

RenderGroup::RenderGroup(
    Handle<Shader> shader,
    const RenderableAttributeSet &renderable_attributes
) : BasicObject(),
    m_shader(std::move(shader)),
    m_pipeline(MakeRenderObject<renderer::GraphicsPipeline>()),
    m_renderable_attributes(renderable_attributes)
{
    if (m_shader != nullptr) {
        m_pipeline->SetShaderProgram(m_shader->GetShaderProgram());
    }
}

RenderGroup::RenderGroup(
    Handle<Shader> shader,
    const RenderableAttributeSet &renderable_attributes,
    DescriptorTableRef descriptor_table
) : BasicObject(),
    // Using old version with descriptor sets
    m_pipeline(MakeRenderObject<renderer::GraphicsPipeline>(ShaderProgramRef::unset, std::move(descriptor_table))),
    m_shader(std::move(shader)),
    m_renderable_attributes(renderable_attributes)
{
    if (m_shader != nullptr) {
        m_pipeline->SetShaderProgram(m_shader->GetShaderProgram());
    }
}

RenderGroup::~RenderGroup()
{
    if (m_indirect_renderer != nullptr) {
        m_indirect_renderer->Destroy();
    }
    
    m_shader.Reset();

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        SafeRelease(std::move(m_command_buffers[frame_index]));
    }

    m_command_buffers = { };

    for (auto &fbo : m_fbos) {
        fbo.Reset();
    }

    SafeRelease(std::move(m_pipeline));
    
    HYP_SYNC_RENDER();
}

void RenderGroup::RemoveFramebuffer(ID<Framebuffer> id)
{
    const auto it = m_fbos.FindIf([&](const auto &item) {
        return item->GetID() == id;
    });

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

    m_indirect_renderer.Reset(new IndirectRenderer());
    m_indirect_renderer->Create();

    AssertThrow(m_fbos.Any());

    for (auto &fbo : m_fbos) {
        AssertThrow(fbo.IsValid());
        InitObject(fbo);
    }

    AssertThrow(m_shader.IsValid());
    InitObject(m_shader);

    for (uint i = 0; i < max_frames_in_flight; i++) {
        for (CommandBufferRef &command_buffer : m_command_buffers[i]) {
            command_buffer = MakeRenderObject<renderer::CommandBuffer>(CommandBufferType::COMMAND_BUFFER_SECONDARY);
        }
    }

    
    RenderPassRef render_pass;
    
    Array<FramebufferObjectRef> framebuffers;
    framebuffers.Reserve(m_fbos.Size());

    for (auto &fbo : m_fbos) {
        if (!render_pass.IsValid()) {
            render_pass = fbo->GetRenderPass();
        }

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            framebuffers.PushBack(fbo->GetFramebuffer(frame_index));
        }
    }

    m_pipeline->SetShaderProgram(m_shader->GetShaderProgram());

    if (!m_pipeline->GetDescriptorTable().IsValid()) {
        renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();

        DescriptorTableRef descriptor_table = MakeRenderObject<renderer::DescriptorTable>(descriptor_table_decl);
        AssertThrow(descriptor_table != nullptr);
        DeferCreate(descriptor_table, g_engine->GetGPUDevice());

        m_pipeline->SetDescriptorTable(std::move(descriptor_table));
    }

    AssertThrow(m_pipeline->GetDescriptorTable().IsValid());

    m_pipeline.SetName(CreateNameFromDynamicString(ANSIString("GraphicsPipeline_") + m_shader->GetCompiledShader().GetName().LookupString()));

    PUSH_RENDER_COMMAND(
        CreateGraphicsPipeline, 
        m_pipeline,
        render_pass,
        std::move(framebuffers),
        m_command_buffers,
        m_renderable_attributes
    );
        
    SetReady(true);
}

void RenderGroup::CollectDrawCalls()
{
    Threads::AssertOnThread(THREAD_RENDER | THREAD_TASK);

    AssertReady();

    m_indirect_renderer->GetDrawState().ResetDrawState();

    m_divided_draw_calls = { };

    DrawCallCollection previous_draw_state = std::move(m_draw_state);

    for (EntityDrawData &entity_draw_data : m_entity_draw_datas) {
        AssertThrow(entity_draw_data.mesh_id.IsValid());

        DrawCallID draw_call_id;

        if constexpr (DrawCall::unique_per_material) {
            draw_call_id = DrawCallID(entity_draw_data.mesh_id, entity_draw_data.material_id);
        } else {
            draw_call_id = DrawCallID(entity_draw_data.mesh_id);
        }

        BufferTicket<EntityInstanceBatch> batch_index = 0;

        if (DrawCall *draw_call = previous_draw_state.TakeDrawCall(draw_call_id)) {
            // take the batch for reuse
            if ((batch_index = draw_call->batch_index)) {
                g_engine->GetRenderData()->entity_instance_batches.ResetBatch(batch_index);
            }

            draw_call->batch_index = 0;
        }

        m_draw_state.PushDrawCall(batch_index, draw_call_id, entity_draw_data);
    }

    previous_draw_state.Reset();

    // register draw calls for indirect rendering
    for (DrawCall &draw_call : m_draw_state.draw_calls) {
        DrawCommandData draw_command_data;
        m_indirect_renderer->GetDrawState().PushDrawCall(draw_call, draw_command_data);
        draw_call.draw_command_index = draw_command_data.draw_command_index;
    }

    m_entity_draw_datas.Clear();
}

void RenderGroup::PerformOcclusionCulling(Frame *frame, const CullData *cull_data)
{
    if constexpr (!use_draw_indirect) {
        return;
    }

    Threads::AssertOnThread(THREAD_RENDER);

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

        // auto &container = out_divided_draw_calls[container_index];
        // container.Reserve(num_draw_calls_divided);

        // for (SizeType i = 0; i < num_draw_calls_divided && draw_call_index < num_draw_calls; i++, draw_call_index++) {
        //     container.PushBack(draw_calls[draw_call_index]);
        // }
    }
}

template <bool IsIndirect>
static HYP_FORCE_INLINE void
RenderAll(
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

    const auto &scene_binding = g_engine->GetRenderState().GetScene();
    const ID<Scene> scene_id = scene_binding.id;

    const uint frame_index = frame->GetFrameIndex();

    const uint num_batches = use_parallel_rendering
        ? MathUtil::Min(uint(TaskSystem::GetInstance().GetPool(THREAD_POOL_RENDER).threads.Size()), num_async_rendering_command_buffers)
        : 1u;
    
    GetDividedDrawCalls(
        draw_state.draw_calls,
        num_async_rendering_command_buffers,
        divided_draw_calls
    );

    // rather than using a single integer, we have to set states in a fixed array
    // because otherwise we'd need to use an atomic integer
    FixedArray<uint, num_async_rendering_command_buffers> command_buffers_recorded_states { };

    const uint global_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(HYP_NAME(Global));
    const DescriptorSet2Ref &global_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index);

    const uint scene_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(HYP_NAME(Scene));
    const DescriptorSet2Ref &scene_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(HYP_NAME(Scene), frame_index);
    
    const uint material_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(HYP_NAME(Material));
    const DescriptorSet2Ref &material_descriptor_set = use_bindless_textures
        ? pipeline->GetDescriptorTable()->GetDescriptorSet(HYP_NAME(Material), frame_index)
        : DescriptorSet2Ref::unset;
    
    const uint entity_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(HYP_NAME(Object));
    const DescriptorSet2Ref &entity_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(HYP_NAME(Object), frame_index);

    // AtomicVar<uint32> num_rendered_objects { 0u };

#if defined(HYP_FEATURES_PARALLEL_RENDERING) && HYP_FEATURES_PARALLEL_RENDERING
    ParallelForEach(divided_draw_calls, num_batches, THREAD_POOL_RENDER,
#else
    ForEachInBatches(divided_draw_calls, num_batches,
#endif
        [&](Span<const DrawCall> draw_calls, uint index, uint)
        {
            if (!draw_calls) {
                return;
            }

            auto &mesh_container = Handle<Mesh>::GetContainer();

            command_buffers[frame_index][index]->Record(
                g_engine->GetGPUDevice(),
                pipeline->GetConstructionInfo().render_pass,
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
                            { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                            { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                            { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                            { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                            { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
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
                                    { HYP_NAME(SkeletonsBuffer), HYP_RENDER_OBJECT_OFFSET(Skeleton, draw_call.skeleton_id.ToIndex()) },
                                    { HYP_NAME(EntityInstanceBatchesBuffer), uint32(draw_call.batch_index * sizeof(EntityInstanceBatch)) }
                                },
                                entity_descriptor_set_index
                            );
#else
                            entity_descriptor_set->Bind(
                                secondary,
                                pipeline,
                                {
                                    { HYP_NAME(MaterialsBuffer), HYP_RENDER_OBJECT_OFFSET(Material, draw_call.material_id.ToIndex()) },
                                    { HYP_NAME(SkeletonsBuffer), HYP_RENDER_OBJECT_OFFSET(Skeleton, draw_call.skeleton_id.ToIndex()) },
                                    { HYP_NAME(EntityInstanceBatchesBuffer), uint32(draw_call.batch_index * sizeof(EntityInstanceBatch)) }
                                },
                                entity_descriptor_set_index
                            );
#endif
                        }

                        // Bind material descriptor set
                        if (material_descriptor_set_index != ~0u && !use_bindless_textures) {
                            g_engine->GetMaterialDescriptorSetManager().GetDescriptorSet(draw_call.material_id, frame_index)
                                ->Bind(secondary, pipeline, material_descriptor_set_index);
                        }

                        if constexpr (IsIndirect) {
                            mesh_container.Get(draw_call.mesh_id.ToIndex())
                                .RenderIndirect(
                                    secondary,
                                    indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index).Get(),
                                    draw_call.draw_command_index * sizeof(IndirectDrawCommand)
                                );
                        } else {
                            mesh_container.Get(draw_call.mesh_id.ToIndex())
                                .Render(secondary, entity_batch.num_entities);
                        }

                        // num_rendered_objects.Increment(1u, MemoryOrder::RELAXED);
                    }

                    HYPERION_RETURN_OK;
                }
            );

            command_buffers_recorded_states[index] = 1u;
        }
    );

    const auto num_recorded_command_buffers = command_buffers_recorded_states.Sum();

    // submit all command buffers
    for (uint i = 0; i < num_recorded_command_buffers; i++) {
        command_buffers[frame_index][i]
            ->SubmitSecondary(frame->GetCommandBuffer());
    }

    command_buffer_index = (command_buffer_index + num_recorded_command_buffers) % uint(command_buffers.Size());

    // DebugLog(
    //     LogType::Debug, "Rendered %u objects for RenderGroup\n",
    //         num_rendered_objects.Get(MemoryOrder::RELAXED));
}

void RenderGroup::PerformRendering(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    if (m_draw_state.draw_calls.Empty()) {
        return;
    }

    RenderAll<false>(
        frame,
        m_command_buffers,
        m_command_buffer_index,
        m_pipeline,
        m_indirect_renderer.Get(),
        m_divided_draw_calls,
        m_draw_state
    );
}

void RenderGroup::PerformRenderingIndirect(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    if (m_draw_state.draw_calls.Empty()) {
        return;
    }

    RenderAll<true>(
        frame,
        m_command_buffers,
        m_command_buffer_index,
        m_pipeline,
        m_indirect_renderer.Get(),
        m_divided_draw_calls,
        m_draw_state
    );
}

void RenderGroup::Render(Frame *frame)
{
    // perform all ops in one batch
    CollectDrawCalls();
    PerformRendering(frame);
}


void RenderGroup::SetEntityDrawDatas(Array<EntityDrawData> entity_draw_datas)
{
    Threads::AssertOnThread(THREAD_RENDER | THREAD_TASK);

    m_entity_draw_datas = std::move(entity_draw_datas);
}

// Proxied methods

const CommandBufferRef &RendererProxy::GetCommandBuffer(uint frame_index) const
{
    return m_render_group->m_command_buffers[frame_index].Front();
}

const GraphicsPipelineRef &RendererProxy::GetGraphicsPipeline() const
{
    return m_render_group->m_pipeline;
}

void RendererProxy::Bind(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    CommandBuffer *command_buffer = m_render_group->m_command_buffers[frame->GetFrameIndex()].Front().Get();
    AssertThrow(command_buffer != nullptr);

    command_buffer->Begin(g_engine->GetGPUDevice(), m_render_group->m_pipeline->GetConstructionInfo().render_pass);

    m_render_group->m_pipeline->Bind(command_buffer);
}

void RendererProxy::DrawMesh(Frame *frame, Mesh *mesh)
{
    CommandBuffer *command_buffer = m_render_group->m_command_buffers[frame->GetFrameIndex()].Front().Get();
    AssertThrow(command_buffer != nullptr);

    mesh->Render(command_buffer);
}

void RendererProxy::Submit(Frame *frame)
{
    CommandBuffer *command_buffer = m_render_group->m_command_buffers[frame->GetFrameIndex()].Front().Get();
    AssertThrow(command_buffer != nullptr);

    command_buffer->End(g_engine->GetGPUDevice());
    command_buffer->SubmitSecondary(frame->GetCommandBuffer());
}

} // namespace hyperion::v2
