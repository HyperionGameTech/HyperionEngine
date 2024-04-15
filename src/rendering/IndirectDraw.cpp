/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include "IndirectDraw.hpp"
#include <rendering/DrawCall.hpp>
#include <rendering/Mesh.hpp>

#include <math/MathUtil.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::ShaderVec2;
using renderer::ShaderVec3;
using renderer::ShaderVec4;

template <class BufferType>
static bool ResizeBuffer(
    Frame *frame,
    const GPUBufferRef &buffer,
    SizeType new_buffer_size
)
{
    if constexpr (IndirectDrawState::use_next_pow2_size) {
        new_buffer_size = MathUtil::NextPowerOf2(new_buffer_size);
    }

    bool size_changed = false;

    HYPERION_ASSERT_RESULT(buffer->EnsureCapacity(
        g_engine->GetGPUDevice(),
        new_buffer_size,
        &size_changed
    ));

    return size_changed;
}

static bool ResizeIndirectDrawCommandsBuffer(
    Frame *frame,
    uint num_draw_commands,
    const GPUBufferRef &indirect_buffer,
    const GPUBufferRef &staging_buffer
)
{
    // assume render thread
    const bool was_created_or_resized = ResizeBuffer<renderer::IndirectBuffer>(
        frame,
        indirect_buffer,
        num_draw_commands * sizeof(IndirectDrawCommand)
    );

    if (!was_created_or_resized) {
        return false;
    }

    // upload zeros to the buffer using a staging buffer.
    if (staging_buffer->IsCreated()) {
        HYPERION_ASSERT_RESULT(staging_buffer->EnsureCapacity(
            g_engine->GetGPUDevice(),
            indirect_buffer->size
        ));
    } else {
        HYPERION_ASSERT_RESULT(staging_buffer->Create(
            g_engine->GetGPUDevice(),
            indirect_buffer->size
        ));
    }

    staging_buffer->Memset(
        g_engine->GetGPUDevice(),
        staging_buffer->size,
        0x0 // fill buffer with zeros
    );

    staging_buffer->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::COPY_SRC
    );

    indirect_buffer->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::COPY_DST
    );

    indirect_buffer->CopyFrom(
        frame->GetCommandBuffer(),
        staging_buffer.Get(),
        staging_buffer->size
    );

    indirect_buffer->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::INDIRECT_ARG
    );

    return true;
}

static bool ResizeInstancesBuffer(
    Frame *frame,
    uint num_object_instances,
    const GPUBufferRef &instance_buffer,
    const GPUBufferRef &staging_buffer
)
{
    // assume render thread

    const bool was_created_or_resized = ResizeBuffer<StorageBuffer>(
        frame,
        instance_buffer,
        num_object_instances * sizeof(ObjectInstance)
    );

    if (was_created_or_resized) {
        instance_buffer->Memset(
            g_engine->GetGPUDevice(),
            instance_buffer->size,
            0x0
        );
    }

    return was_created_or_resized;
}

static bool ResizeIfNeeded(
    Frame *frame,
    const FixedArray<GPUBufferRef, max_frames_in_flight> &indirect_buffers,
    const FixedArray<GPUBufferRef, max_frames_in_flight> &instance_buffers,
    const FixedArray<GPUBufferRef, max_frames_in_flight> &staging_buffers,
    uint num_draw_commands,
    uint num_object_instances,
    uint8 dirty_bits
)
{
    // assume render thread

    bool resize_happened = false;

    const GPUBufferRef &indirect_buffer = indirect_buffers[frame->GetFrameIndex()];
    const GPUBufferRef &instance_buffer = instance_buffers[frame->GetFrameIndex()];
    const GPUBufferRef &staging_buffer = staging_buffers[frame->GetFrameIndex()];
    
    if ((dirty_bits & (1u << frame->GetFrameIndex())) || !indirect_buffers[frame->GetFrameIndex()].IsValid()) {
        resize_happened |= ResizeIndirectDrawCommandsBuffer(frame, num_draw_commands, indirect_buffer, staging_buffer);
    }
    
    if ((dirty_bits & (1u << frame->GetFrameIndex())) || !instance_buffers[frame->GetFrameIndex()].IsValid()) {
        resize_happened |= ResizeInstancesBuffer(frame, num_object_instances, instance_buffer, staging_buffer);
    }

    return resize_happened;
}

#pragma region Render commands

struct RENDER_COMMAND(CreateIndirectDrawStateBuffers) : renderer::RenderCommand
{
    FixedArray<GPUBufferRef, max_frames_in_flight>  indirect_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight>  instance_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight>  staging_buffers;

    RENDER_COMMAND(CreateIndirectDrawStateBuffers)(
        const FixedArray<GPUBufferRef, max_frames_in_flight> &indirect_buffers,
        const FixedArray<GPUBufferRef, max_frames_in_flight> &instance_buffers,
        const FixedArray<GPUBufferRef, max_frames_in_flight> &staging_buffers
    ) : indirect_buffers(indirect_buffers),
        instance_buffers(instance_buffers),
        staging_buffers(staging_buffers)
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            AssertThrow(this->indirect_buffers[frame_index].IsValid());
            AssertThrow(this->instance_buffers[frame_index].IsValid());
            AssertThrow(this->staging_buffers[frame_index].IsValid());
        }
    }

    virtual ~RENDER_COMMAND(CreateIndirectDrawStateBuffers)() override = default;

    virtual Result operator()() override
    {
        auto single_time_commands = g_engine->GetGPUInstance()->GetSingleTimeCommands();

        single_time_commands.Push([this](const CommandBufferRef &command_buffer) -> Result
        {
            for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                Frame frame = Frame::TemporaryFrame(command_buffer, frame_index);

                if (!ResizeIndirectDrawCommandsBuffer(&frame, IndirectDrawState::initial_count, indirect_buffers[frame_index], staging_buffers[frame_index])) {
                    return { Result::RENDERER_ERR, "Failed to create indirect draw commands buffer!" };
                }

                if (!ResizeInstancesBuffer(&frame, IndirectDrawState::initial_count, instance_buffers[frame_index], staging_buffers[frame_index])) {
                    return { Result::RENDERER_ERR, "Failed to create instances buffer!" };
                }
            }

            HYPERION_RETURN_OK;
        });

        return single_time_commands.Execute(g_engine->GetGPUDevice());
    }
};

#pragma endregion

IndirectDrawState::IndirectDrawState()
    : m_num_draw_commands(0),
      m_dirty_bits(0x3)
{
    // Allocate used buffers so they can be set in descriptor sets
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_indirect_buffers[frame_index] = MakeRenderObject<GPUBuffer>(renderer::IndirectBuffer());
        m_instance_buffers[frame_index] = MakeRenderObject<GPUBuffer>(renderer::StorageBuffer());
        m_staging_buffers[frame_index] = MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STAGING_BUFFER);
    }
}

IndirectDrawState::~IndirectDrawState()
{
}

void IndirectDrawState::Create()
{
    // Push render command to create buffers
    PUSH_RENDER_COMMAND(
        CreateIndirectDrawStateBuffers,
        m_indirect_buffers,
        m_instance_buffers,
        m_staging_buffers
    );
}

void IndirectDrawState::Destroy()
{
    SafeRelease(std::move(m_indirect_buffers));
    SafeRelease(std::move(m_instance_buffers));
    SafeRelease(std::move(m_staging_buffers));
}

void IndirectDrawState::PushDrawCall(const DrawCall &draw_call, DrawCommandData &out)
{
    // assume render thread

    out = { };

    const uint32 draw_command_index = m_num_draw_commands++;

    for (uint index = 0; index < draw_call.entity_id_count; index++) {
        ObjectInstance instance { };
        instance.entity_id = draw_call.entity_ids[index].Value();
        instance.draw_command_index = draw_command_index;
        instance.instance_index = index;
        instance.batch_index = draw_call.batch_index;

        m_object_instances.PushBack(instance);
    }

    out.draw_command_index = draw_command_index;

    if (m_draw_commands.Size() < SizeType(m_num_draw_commands)) {
        m_draw_commands.Resize(SizeType(m_num_draw_commands));
    }

    Handle<Mesh>::GetContainer().Get(draw_call.mesh_id.ToIndex())
        .PopulateIndirectDrawCommand(m_draw_commands[draw_command_index]);
    
    m_dirty_bits |= 0x3;
}

void IndirectDrawState::ResetDrawState()
{
    // assume render thread

    m_num_draw_commands = 0;

    m_object_instances.Clear();
    m_draw_commands.Clear();

    m_dirty_bits |= 0x3;
}

void IndirectDrawState::UpdateBufferData(Frame *frame, bool *out_was_resized)
{
    // assume render thread

    const uint frame_index = frame->GetFrameIndex();

    if ((*out_was_resized = ResizeIfNeeded(
        frame,
        m_indirect_buffers,
        m_instance_buffers,
        m_staging_buffers,
        m_num_draw_commands,
        m_object_instances.Size(),
        m_dirty_bits
    ))) {
        m_dirty_bits |= (1u << frame_index);
    }

    if (!(m_dirty_bits & (1u << frame_index))) {
        return;
    }
    
    // fill instances buffer with data of the meshes
    {
        AssertThrow(m_staging_buffers[frame_index].IsValid());
        AssertThrow(m_staging_buffers[frame_index]->size >= sizeof(IndirectDrawCommand) * m_draw_commands.Size());
        
        m_staging_buffers[frame_index]->Copy(
            g_engine->GetGPUDevice(),
            m_draw_commands.Size() * sizeof(IndirectDrawCommand),
            m_draw_commands.Data()
        );

        m_staging_buffers[frame_index]->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::COPY_SRC
        );

        m_indirect_buffers[frame_index]->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::COPY_DST
        );

        m_indirect_buffers[frame_index]->CopyFrom(
            frame->GetCommandBuffer(),
            m_staging_buffers[frame_index].Get(),
            m_staging_buffers[frame_index]->size
        );

        m_indirect_buffers[frame_index]->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::INDIRECT_ARG
        );
    }

    AssertThrow(m_instance_buffers[frame_index]->size >= m_object_instances.Size() * sizeof(ObjectInstance));

    // update data for object instances (cpu - gpu)
    m_instance_buffers[frame_index]->Copy(
        g_engine->GetGPUDevice(),
        m_object_instances.Size() * sizeof(ObjectInstance),
        m_object_instances.Data()
    );
    
    m_dirty_bits &= ~(1u << frame_index);
}

IndirectRenderer::IndirectRenderer()
    : m_cached_cull_data_updated_bits(0x0)
{
}

IndirectRenderer::~IndirectRenderer() = default;

void IndirectRenderer::Create()
{
    m_indirect_draw_state.Create();

    Handle<Shader> object_visibility_shader = g_shader_manager->GetOrCreate(HYP_NAME(ObjectVisibility));
    AssertThrow(object_visibility_shader.IsValid());

    renderer::DescriptorTableDeclaration descriptor_table_decl = object_visibility_shader->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<renderer::DescriptorTable>(descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSet2Ref &descriptor_set = descriptor_table->GetDescriptorSet(HYP_NAME(ObjectVisibilityDescriptorSet), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(HYP_NAME(ObjectInstancesBuffer), m_indirect_draw_state.GetInstanceBuffer(frame_index));
        descriptor_set->SetElement(HYP_NAME(IndirectDrawCommandsBuffer), m_indirect_draw_state.GetIndirectBuffer(frame_index));
        descriptor_set->SetElement(HYP_NAME(EntityInstanceBatchesBuffer), g_engine->GetRenderData()->entity_instance_batches.GetBuffer());
    }

    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    m_object_visibility = MakeRenderObject<renderer::ComputePipeline>(
        object_visibility_shader->GetShaderProgram(),
        descriptor_table
    );
    
    // use DeferCreate because our Shader might not be ready yet
    DeferCreate(m_object_visibility, g_engine->GetGPUDevice());
}

void IndirectRenderer::Destroy()
{
    SafeRelease(std::move(m_object_visibility));

    m_indirect_draw_state.Destroy();
}

void IndirectRenderer::ExecuteCullShaderInBatches(Frame *frame, const CullData &cull_data)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();

    AssertThrow(m_indirect_draw_state.GetIndirectBuffer(frame_index).IsValid());
    AssertThrow(m_indirect_draw_state.GetIndirectBuffer(frame_index)->size != 0);

    const uint num_instances = uint(m_indirect_draw_state.GetInstances().Size());
    const uint num_batches = (num_instances / IndirectDrawState::batch_size) + 1;

    if (num_instances == 0) {
        return;
    }

    {
        bool was_buffer_resized = false;
        m_indirect_draw_state.UpdateBufferData(frame, &was_buffer_resized);

        if (was_buffer_resized) {
            RebuildDescriptors(frame);
        }
    }

    if (m_cached_cull_data != cull_data) {
        m_cached_cull_data = cull_data;
        m_cached_cull_data_updated_bits = 0xFF;
    }

    // if (m_cached_cull_data_updated_bits & (1u << frame_index)) {
    //     m_descriptor_sets[frame_index]->GetDescriptor(6)
    //         ->SetElementSRV(0, m_cached_cull_data.depth_pyramid_image_view);

    //     m_descriptor_sets[frame_index]->ApplyUpdates(g_engine->GetGPUDevice());

    //     m_cached_cull_data_updated_bits &= ~(1u << frame_index);
    // }
    
    const ID<Scene> scene_id = g_engine->GetRenderState().GetScene().id;
    const uint scene_index = scene_id.ToIndex();

    m_object_visibility->GetDescriptorTable()->Bind(
        frame,
        m_object_visibility,
        {
            {
                HYP_NAME(Scene),
                {
                    { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                    { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                    { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                    { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );
    
    m_indirect_draw_state.GetIndirectBuffer(frame_index)->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::INDIRECT_ARG
    );

    m_object_visibility->Bind(command_buffer, Pipeline::PushConstantData {
        .object_visibility_data = {
            .batch_offset = 0,
            .num_instances = num_instances,
            .scene_id = uint32(scene_id.value),
            .depth_pyramid_dimensions = Extent2D(g_engine->GetDeferredRenderer().GetDepthPyramidRenderer().GetExtent())
        }
    });
    
    m_object_visibility->Dispatch(command_buffer, Extent3D { num_batches, 1, 1 });
    
    m_indirect_draw_state.GetIndirectBuffer(frame_index)->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::INDIRECT_ARG
    );
}

void IndirectRenderer::RebuildDescriptors(Frame *frame)
{
    const uint frame_index = frame->GetFrameIndex();

    const DescriptorTableRef &descriptor_table = m_object_visibility->GetDescriptorTable();

    const DescriptorSet2Ref &descriptor_set = descriptor_table->GetDescriptorSet(HYP_NAME(ObjectVisibilityDescriptorSet), frame_index);
    AssertThrow(descriptor_set != nullptr);

    descriptor_set->SetElement(HYP_NAME(ObjectInstancesBuffer), m_indirect_draw_state.GetInstanceBuffer(frame_index));
    descriptor_set->SetElement(HYP_NAME(IndirectDrawCommandsBuffer), m_indirect_draw_state.GetIndirectBuffer(frame_index));

    descriptor_set->Update(g_engine->GetGPUDevice());
}

} // namespace hyperion::v2