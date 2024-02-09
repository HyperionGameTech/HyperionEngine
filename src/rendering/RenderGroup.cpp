#include "RenderGroup.hpp"
#include <scene/Entity.hpp>
#include <Engine.hpp>
#include <Constants.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>

namespace hyperion::v2 {

using renderer::Result;
using renderer::CommandBufferType;

#pragma region Render commands

struct RENDER_COMMAND(CreateGraphicsPipeline) : renderer::RenderCommand
{
    GraphicsPipelineRef                     pipeline;
    renderer::ShaderProgram                 *shader_program;
    RenderPassRef                           render_pass;
    Array<FramebufferObjectRef>             framebuffers;
    Array<Array<renderer::CommandBuffer *>> command_buffers;
    RenderableAttributeSet                  attributes;

    RENDER_COMMAND(CreateGraphicsPipeline)(
        GraphicsPipelineRef pipeline,
        renderer::ShaderProgram *shader_program,
        RenderPassRef render_pass,
        Array<FramebufferObjectRef> &&framebuffers,
        Array<Array<renderer::CommandBuffer *>> &&command_buffers,
        const RenderableAttributeSet &attributes
    ) : pipeline(std::move(pipeline)),
        shader_program(shader_program),
        render_pass(std::move(render_pass)),
        framebuffers(std::move(framebuffers)),
        command_buffers(std::move(command_buffers)),
        attributes(attributes)
    {
    }

    virtual Result operator()()
    {
        renderer::GraphicsPipeline::ConstructionInfo construction_info {
            .vertex_attributes = attributes.GetMeshAttributes().vertex_attributes,
            .topology          = attributes.GetMeshAttributes().topology,
            .cull_mode         = attributes.GetMaterialAttributes().cull_faces,
            .fill_mode         = attributes.GetMaterialAttributes().fill_mode,
            .blend_mode        = attributes.GetMaterialAttributes().blend_mode,
            .depth_test        = bool(attributes.GetMaterialAttributes().flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST),
            .depth_write       = bool(attributes.GetMaterialAttributes().flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE),
            .shader            = shader_program,
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
                    g_engine->GetGPUInstance()->GetGraphicsCommandPool(j)
                ));
            }
        }

        return pipeline->Create(
            g_engine->GetGPUDevice(),
            std::move(construction_info),
            &g_engine->GetGPUInstance()->GetDescriptorPool()
        );
    }
};

#pragma endregion

RenderGroup::RenderGroup(
    Handle<Shader> &&shader,
    const RenderableAttributeSet &renderable_attributes
) : BasicObject(),
    m_pipeline(MakeRenderObject<renderer::GraphicsPipeline>()),
    m_shader(std::move(shader)),
    m_renderable_attributes(renderable_attributes)
{
}

RenderGroup::RenderGroup(
    Handle<Shader> &&shader,
    const RenderableAttributeSet &renderable_attributes,
    const Array<DescriptorSetRef> &used_descriptor_sets
) : BasicObject(),
    m_pipeline(MakeRenderObject<renderer::GraphicsPipeline>(used_descriptor_sets)),
    m_shader(std::move(shader)),
    m_renderable_attributes(renderable_attributes)
{
}

RenderGroup::~RenderGroup()
{
    Teardown();
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

    // create our indirect renderer
    // will be created with some initial size.
    m_indirect_renderer.Create();

    AssertThrow(m_fbos.Any());

    for (auto &fbo : m_fbos) {
        AssertThrow(fbo.IsValid());
        InitObject(fbo);
    }

    AssertThrow(m_shader.IsValid());
    InitObject(m_shader);

    for (uint i = 0; i < max_frames_in_flight; i++) {
        for (auto &command_buffer : m_command_buffers[i]) {
            command_buffer.Reset(new CommandBuffer(CommandBufferType::COMMAND_BUFFER_SECONDARY));
        }
    }

    // Do we need the callback anymore?
    // @TODO: Refactor how global descriptor sets are created,
    // and see if this can be removed
    OnInit(g_engine->callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this](...) {
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

        Array<Array<renderer::CommandBuffer *>> command_buffers;
        command_buffers.Reserve(m_command_buffers.Size());
        
        for (auto &item : m_command_buffers) {
            Array<renderer::CommandBuffer *> frame_command_buffers;
            frame_command_buffers.Reserve(item.Size());

            for (auto &command_buffer : item) {
                frame_command_buffers.PushBack(command_buffer.Get());
            }

            command_buffers.PushBack(std::move(frame_command_buffers));
        }

        PUSH_RENDER_COMMAND(
            CreateGraphicsPipeline, 
            m_pipeline,
            m_shader->GetShaderProgram(),
            render_pass,
            std::move(framebuffers),
            std::move(command_buffers),
            m_renderable_attributes
        );
            
        SetReady(true);

        OnTeardown([this]() {
            SetReady(false);

            m_indirect_renderer.Destroy(); // make sure we have the render queue flush at the end of
                                                 // this, as the indirect renderer has a call back that needs to be exec'd
                                                 // before the destructor is called

            m_shader.Reset();

            for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                for (uint i = 0; i < uint(m_command_buffers[frame_index].Size()); i++) {
                    g_safe_deleter->SafeRelease(std::move(m_command_buffers[frame_index][i]));
                }
            }

            for (auto &fbo : m_fbos) {
                fbo.Reset();
            }

            SafeRelease(std::move(m_pipeline));
            
            HYP_SYNC_RENDER();
        });
    }));
}

void RenderGroup::CollectDrawCalls()
{
    Threads::AssertOnThread(THREAD_RENDER | THREAD_TASK);

    AssertReady();

    m_indirect_renderer.GetDrawState().Reset();
    m_divided_draw_calls.Clear();

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
        m_indirect_renderer.GetDrawState().PushDrawCall(draw_call, draw_command_data);
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

    m_indirect_renderer.ExecuteCullShaderInBatches(
        frame,
        *cull_data
    );
}

static void GetDividedDrawCalls(
    const Array<DrawCall> &draw_calls,
    uint num_batches,
    Array<Array<DrawCall>> &out_divided_draw_calls
)
{
    out_divided_draw_calls.Resize(num_batches);

    const uint num_draw_calls = uint(draw_calls.Size());
    const uint num_draw_calls_divided = (num_draw_calls + num_batches - 1) / num_batches;

    uint draw_call_index = 0;

    for (SizeType container_index = 0; container_index < num_async_rendering_command_buffers; container_index++) {
        auto &container = out_divided_draw_calls[container_index];
        container.Reserve(num_draw_calls_divided);

        for (SizeType i = 0; i < num_draw_calls_divided && draw_call_index < num_draw_calls; i++, draw_call_index++) {
            container.PushBack(draw_calls[draw_call_index]);
        }
    }
}

static void BindGlobalDescriptorSets(
    Frame *frame,
    renderer::GraphicsPipeline *pipeline,
    CommandBuffer *command_buffer
)
{
    const uint frame_index = frame->GetFrameIndex();

    command_buffer->BindDescriptorSets(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        pipeline,
        FixedArray<DescriptorSet::Index, 2> { DescriptorSet::global_buffer_mapping[frame_index], DescriptorSet::scene_buffer_mapping[frame_index] },
        FixedArray<DescriptorSet::Index, 2> { DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL, DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE },
        FixedArray {
            HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(Light, 0),
            HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
        }
    );

#if HYP_FEATURES_BINDLESS_TEXTURES
    /* Bindless textures */
    g_engine->GetGPUInstance()->GetDescriptorPool().Bind(
        g_engine->GetGPUDevice(),
        command_buffer,
        pipeline,
        {
            {.set = DescriptorSet::bindless_textures_mapping[frame_index], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS}
        }
    );
#endif
                    
    g_engine->GetGPUInstance()->GetDescriptorPool().Bind(
        g_engine->GetGPUDevice(),
        command_buffer,
        pipeline,
        {
            {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER, .count = 1},
        }
    );
}

static void BindPerObjectDescriptorSets(
    Frame *frame,
    renderer::GraphicsPipeline *pipeline,
    CommandBuffer *command_buffer,
    uint batch_index,
    uint skeleton_index,
    uint material_index
)
{
    const uint frame_index = frame->GetFrameIndex();
    
#if HYP_FEATURES_BINDLESS_TEXTURES
    if constexpr (use_indexed_array_for_object_data) {
        command_buffer->BindDescriptorSets(
            g_engine->GetGPUInstance()->GetDescriptorPool(),
            pipeline,
            FixedArray<DescriptorSet::Index, 1> { DescriptorSet::object_buffer_mapping[frame_index] },
            FixedArray<DescriptorSet::Index, 1> { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT },
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Skeleton, skeleton_index),
                uint32(batch_index * sizeof(EntityInstanceBatch))
            }
        );
    } else {
        command_buffer->BindDescriptorSets(
            g_engine->GetGPUInstance()->GetDescriptorPool(),
            pipeline,
            FixedArray<DescriptorSet::Index, 1> { DescriptorSet::object_buffer_mapping[frame_index] },
            FixedArray<DescriptorSet::Index, 1> { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT },
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Material, material_index),
                HYP_RENDER_OBJECT_OFFSET(Skeleton, skeleton_index),
                uint32(batch_index * sizeof(EntityInstanceBatch))
            }
        );
    }
#else
    if constexpr (use_indexed_array_for_object_data) {
        command_buffer->BindDescriptorSets(
            g_engine->GetGPUInstance()->GetDescriptorPool(),
            pipeline,
            FixedArray<DescriptorSet::Index, 2> { DescriptorSet::object_buffer_mapping[frame_index], DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, material_index, frame_index) },
            FixedArray<DescriptorSet::Index, 2> { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT, DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES },
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Skeleton, skeleton_index),
                uint32(batch_index * sizeof(EntityInstanceBatch))
            }
        );
    } else {
        command_buffer->BindDescriptorSets(
            g_engine->GetGPUInstance()->GetDescriptorPool(),
            pipeline,
            FixedArray<DescriptorSet::Index, 2> { DescriptorSet::object_buffer_mapping[frame_index], DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, material_index, frame_index) },
            FixedArray<DescriptorSet::Index, 2> { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT, DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES },
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Material, material_index),
                HYP_RENDER_OBJECT_OFFSET(Skeleton, skeleton_index),
                uint32(batch_index * sizeof(EntityInstanceBatch))
            }
        );
    }
#endif
}

template <bool IsIndirect>
static HYP_FORCE_INLINE void
RenderAll(
    Frame *frame,
    FixedArray<FixedArray<UniquePtr<CommandBuffer>, num_async_rendering_command_buffers>, max_frames_in_flight> &command_buffers,
    uint &command_buffer_index,
    const GraphicsPipelineRef &pipeline,
    IndirectRenderer *indirect_renderer,
    Array<Array<DrawCall>> &divided_draw_calls,
    const DrawCallCollection &draw_state
)
{
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
    
    // always run renderer items as HIGH priority,
    // so we do not lock up because we're waiting for a large process to
    // complete in the same thread
    TaskSystem::GetInstance().ParallelForEach(
        THREAD_POOL_RENDER,
        num_batches,
        divided_draw_calls,
        [frame, pipeline, indirect_renderer, &command_buffers, &command_buffers_recorded_states, frame_index](const Array<DrawCall> &draw_calls, uint index, uint)
        {
            if (draw_calls.Empty()) {
                return;
            }

            auto &mesh_container = GetContainer<Mesh>();

            command_buffers[frame_index][index]->Record(
                g_engine->GetGPUDevice(),
                pipeline->GetConstructionInfo().render_pass,
                [&](CommandBuffer *secondary)
                {
                    pipeline->Bind(secondary);

                    BindGlobalDescriptorSets(
                        frame,
                        pipeline,
                        secondary
                    );

                    for (const DrawCall &draw_call : draw_calls) {
                        const EntityInstanceBatch &entity_batch = g_engine->GetRenderData()->entity_instance_batches.Get(draw_call.batch_index);
#ifdef HYP_DEBUG_MODE
                        AssertThrow(draw_call.mesh_id.IsValid());
                        AssertThrow(mesh_container.GetPointer(draw_call.mesh_id.ToIndex()) != nullptr);
#endif


                        BindPerObjectDescriptorSets(
                            frame,
                            pipeline,
                            secondary,
                            draw_call.batch_index,
                            draw_call.skeleton_id.ToIndex(),
                            draw_call.material_id.ToIndex()
                        );

                        if constexpr (IsIndirect) {
#ifdef HYP_DEBUG_MODE
                            AssertThrow(draw_call.draw_command_index * sizeof(IndirectDrawCommand) < indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index)->size);
#endif
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
        command_buffers[frame_index][/*(command_buffer_index + i) % static_cast<uint>(command_buffers.Size())*/ i]
            ->SubmitSecondary(frame->GetCommandBuffer());
    }

    command_buffer_index = (command_buffer_index + num_recorded_command_buffers) % uint(command_buffers.Size());
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
        &m_indirect_renderer,
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
        &m_indirect_renderer,
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

CommandBuffer *RendererProxy::GetCommandBuffer(uint frame_index)
{
    return m_render_group->m_command_buffers[frame_index].Front().Get();
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
