#include "IndirectDraw.hpp"
#include <rendering/DrawCall.hpp>
#include <rendering/Mesh.hpp>

#include <math/MathUtil.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::ShaderVec2;
using renderer::ShaderVec3;
using renderer::ShaderVec4;

class IndirectRenderer;

struct RENDER_COMMAND(CreateIndirectRenderer) : renderer::RenderCommand
{
    IndirectRenderer &indirect_renderer;

    RENDER_COMMAND(CreateIndirectRenderer)(IndirectRenderer &indirect_renderer)
        : indirect_renderer(indirect_renderer)
    {
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(indirect_renderer.m_indirect_draw_state.Create());

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            AssertThrow(indirect_renderer.m_descriptor_sets[frame_index].IsValid());

            // global object data
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(0)
                ->SetElementBuffer(g_engine->GetRenderData()->objects.GetBuffer());

            // global scene data
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
                ->SetElementBuffer<SceneShaderData>(g_engine->GetRenderData()->scenes.GetBuffer());

            // current camera
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(2)
                ->SetElementBuffer<CameraShaderData>(g_engine->GetRenderData()->cameras.GetBuffer());

            // instances buffer
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(3)
                ->SetElementBuffer(indirect_renderer.m_indirect_draw_state.GetInstanceBuffer(frame_index));

            // indirect commands
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(4)
                ->SetElementBuffer(indirect_renderer.m_indirect_draw_state.GetIndirectBuffer(frame_index));

            // entity batches
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(5)
                ->SetElementBuffer(g_engine->GetRenderData()->entity_instance_batches.GetBuffer());

            // depth pyramid image (set to placeholder)
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::ImageDescriptor>(6)
                ->SetSubDescriptor({
                    .image_view = &g_engine->GetPlaceholderData().GetImageView2D1x1R8()
                });

            // sampler
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::SamplerDescriptor>(7)
                ->SetSubDescriptor({
                    .sampler = &g_engine->GetPlaceholderData().GetSamplerNearest()
                });

            HYPERION_BUBBLE_ERRORS(indirect_renderer.m_descriptor_sets[frame_index]->Create(
                g_engine->GetGPUDevice(),
                &g_engine->GetGPUInstance()->GetDescriptorPool()
            ));
        }

        HYPERION_RETURN_OK;
    }
};

IndirectDrawState::IndirectDrawState()
    : m_num_draw_commands(0)
{
}

IndirectDrawState::~IndirectDrawState()
{
}

Result IndirectDrawState::Create()
{
    auto single_time_commands = g_engine->GetGPUInstance()->GetSingleTimeCommands();

    single_time_commands.Push([this](const CommandBufferRef &command_buffer) -> Result {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto frame = Frame::TemporaryFrame(command_buffer, frame_index);

            if (!ResizeIndirectDrawCommandsBuffer(&frame, initial_count)) {
                return { Result::RENDERER_ERR, "Failed to create indirect draw commands buffer!" };
            }

            if (!ResizeInstancesBuffer(&frame, initial_count)) {
                return { Result::RENDERER_ERR, "Failed to create instances buffer!" };
            }
        }

        HYPERION_RETURN_OK;
    });

    return single_time_commands.Execute(g_engine->GetGPUDevice());
}

void IndirectDrawState::Destroy()
{
    SafeRelease(std::move(m_indirect_buffers));
    SafeRelease(std::move(m_instance_buffers));
    SafeRelease(std::move(m_staging_buffers));
}

template <class BufferType>
static bool ResizeBuffer(
    Frame *frame,
    FixedArray<GPUBufferRef, max_frames_in_flight> &buffers,
    SizeType new_buffer_size
)
{
    const auto frame_index = frame->GetFrameIndex();

    SizeType current_buffer_size = 0u;

    bool needs_create = false;

    if (buffers[frame_index].IsValid()) {
        current_buffer_size = buffers[frame_index]->size;

        if (new_buffer_size > current_buffer_size) {
            SafeRelease(std::move(buffers[frame_index]));

            needs_create = true;
        }
    } else {
        needs_create = true;
    }

    if (needs_create) {
        if constexpr (IndirectDrawState::use_next_pow2_size) {
            new_buffer_size = MathUtil::NextPowerOf2(new_buffer_size);
        }

        buffers[frame_index] = MakeRenderObject<GPUBuffer>(BufferType());

        DebugLog(
            LogType::Debug,
            "Resize indirect buffer (%p) at frame index %u from %llu -> %llu\n",
            buffers[frame_index].Get(),
            frame_index,
            current_buffer_size,
            new_buffer_size
        );

        HYPERION_ASSERT_RESULT(buffers[frame_index]->Create(g_engine->GetGPUDevice(), new_buffer_size));

        return true;
    }

    return false;
}

void IndirectDrawState::PushDrawCall(const DrawCall &draw_call, DrawCommandData &out)
{
    out = { };

    const UInt32 draw_command_index = m_num_draw_commands++;

    for (UInt index = 0; index < draw_call.entity_id_count; index++) {
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

    GetContainer<Mesh>().Get(draw_call.mesh_id.ToIndex())
        .PopulateIndirectDrawCommand(m_draw_commands[draw_command_index]);
    
    m_is_dirty = { true, true };
}

bool IndirectDrawState::ResizeIndirectDrawCommandsBuffer(Frame *frame, SizeType count)
{
    const bool was_created_or_resized = ResizeBuffer<renderer::IndirectBuffer>(
        frame,
        m_indirect_buffers,
        count * sizeof(IndirectDrawCommand)
    );

    if (!was_created_or_resized) {
        return false;
    }

    // upload zeros to the buffer using a staging buffer.
    if (m_staging_buffers[frame->GetFrameIndex()].IsValid()) {
        SafeRelease(std::move(m_staging_buffers[frame->GetFrameIndex()]));
    }

    m_staging_buffers[frame->GetFrameIndex()] = MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STAGING_BUFFER);

    HYPERION_ASSERT_RESULT(m_staging_buffers[frame->GetFrameIndex()]->Create(
        g_engine->GetGPUDevice(),
        m_indirect_buffers[frame->GetFrameIndex()]->size
    ));

    m_staging_buffers[frame->GetFrameIndex()]->Memset(
        g_engine->GetGPUDevice(),
        m_staging_buffers[frame->GetFrameIndex()]->size,
        0x00 // fill buffer with zeros
    );

    m_staging_buffers[frame->GetFrameIndex()]->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::COPY_SRC
    );

    m_indirect_buffers[frame->GetFrameIndex()]->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::COPY_DST
    );

    m_indirect_buffers[frame->GetFrameIndex()]->CopyFrom(
        frame->GetCommandBuffer(),
        m_staging_buffers[frame->GetFrameIndex()].Get(),
        m_staging_buffers[frame->GetFrameIndex()]->size
    );

    m_indirect_buffers[frame->GetFrameIndex()]->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::INDIRECT_ARG
    );

    return true;
}

bool IndirectDrawState::ResizeInstancesBuffer(Frame *frame, SizeType count)
{
    const bool was_created_or_resized = ResizeBuffer<StorageBuffer>(
        frame,
        m_instance_buffers,
        count * sizeof(ObjectInstance)
    );

    if (was_created_or_resized) {
        m_instance_buffers[frame->GetFrameIndex()]->Memset(
            g_engine->GetGPUDevice(),
            m_instance_buffers[frame->GetFrameIndex()]->size,
            0x00
        );
    }

    return was_created_or_resized;
}

bool IndirectDrawState::ResizeIfNeeded(Frame *frame)
{
    // assume render thread

    bool resize_happened = false;
    
    if (m_is_dirty[frame->GetFrameIndex()] || !m_indirect_buffers[frame->GetFrameIndex()].IsValid()) {
        resize_happened |= ResizeIndirectDrawCommandsBuffer(frame, m_num_draw_commands);
    }
    
    if (m_is_dirty[frame->GetFrameIndex()] || !m_instance_buffers[frame->GetFrameIndex()].IsValid()) {
        resize_happened |= ResizeInstancesBuffer(frame, m_object_instances.Size());
    }

    return resize_happened;
}

void IndirectDrawState::Reset()
{
    // assume render thread

    m_num_draw_commands = 0;

    m_object_instances.Clear();
    m_draw_commands.Clear();

    m_is_dirty = { true, true };
}

void IndirectDrawState::Reserve(Frame *frame, SizeType count)
{
    // assume render thread

    m_object_instances.Reserve(count);
    m_draw_commands.Reserve(count);

    bool resize_happened = false;
    
    resize_happened |= ResizeIndirectDrawCommandsBuffer(frame, count);
    resize_happened |= ResizeInstancesBuffer(frame, count);

    m_is_dirty[frame->GetFrameIndex()] |= resize_happened;
}

void IndirectDrawState::UpdateBufferData(Frame *frame, bool *out_was_resized)
{
    // assume render thread

    const UInt frame_index = frame->GetFrameIndex();

    if ((*out_was_resized = ResizeIfNeeded(frame))) {
        m_is_dirty[frame_index] = true;
    }

    if (!m_is_dirty[frame_index]) {
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
    
    m_is_dirty[frame_index] = false;
}

IndirectRenderer::IndirectRenderer()
    : m_cached_cull_data_updated_bits(0x0)
{
}

IndirectRenderer::~IndirectRenderer() = default;

void IndirectRenderer::Create()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_descriptor_sets[frame_index] = MakeRenderObject<DescriptorSet>();
    }

    PUSH_RENDER_COMMAND(CreateIndirectRenderer, *this);

    // create compute pipeline for object visibility (for indirect render)
    // TODO: cache pipelines: re-use this
    m_object_visibility = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(HYP_NAME(ObjectVisibility)),
        Array<DescriptorSetRef> { m_descriptor_sets[0] }
    );

    InitObject(m_object_visibility);
    
    //HYP_SYNC_RENDER();
}

void IndirectRenderer::Destroy()
{
    m_object_visibility.Reset();
    
    SafeRelease(std::move(m_descriptor_sets));

    m_indirect_draw_state.Destroy();
}

void IndirectRenderer::ExecuteCullShaderInBatches(Frame *frame, const CullData &cull_data)
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *command_buffer = frame->GetCommandBuffer();
    const UInt frame_index = frame->GetFrameIndex();

    AssertThrow(m_indirect_draw_state.GetIndirectBuffer(frame_index).IsValid());
    AssertThrow(m_indirect_draw_state.GetIndirectBuffer(frame_index)->size != 0);

    const UInt num_instances = UInt(m_indirect_draw_state.GetInstances().Size());
    const UInt num_batches = (num_instances / IndirectDrawState::batch_size) + 1;

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

    if (m_cached_cull_data_updated_bits & (1u << frame_index)) {
        m_descriptor_sets[frame_index]->GetDescriptor(6)
            ->SetElementSRV(0, m_cached_cull_data.depth_pyramid_image_view);

        m_descriptor_sets[frame_index]->ApplyUpdates(g_engine->GetGPUDevice());

        m_cached_cull_data_updated_bits &= ~(1u << frame_index);
    }
    
    const ID<Scene> scene_id = g_engine->GetRenderState().GetScene().id;
    const UInt scene_index = scene_id.ToIndex();

    // bind our descriptor set to binding point 0
    command_buffer->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_object_visibility->GetPipeline(),
        m_descriptor_sets[frame_index],
        static_cast<DescriptorSet::Index>(0),
        FixedArray {
            HYP_RENDER_OBJECT_OFFSET(Scene, scene_index),
            HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
        }
    );
    
    m_indirect_draw_state.GetIndirectBuffer(frame_index)->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::INDIRECT_ARG
    );

    m_object_visibility->GetPipeline()->Bind(command_buffer, Pipeline::PushConstantData {
        .object_visibility_data = {
            .batch_offset = 0,
            .num_instances = num_instances,
            .scene_id = UInt32(scene_id.value),
            .depth_pyramid_dimensions = Extent2D(m_cached_cull_data.depth_pyramid_dimensions)
        }
    });
    
    m_object_visibility->GetPipeline()->Dispatch(command_buffer, Extent3D { num_batches, 1, 1 });
    
    m_indirect_draw_state.GetIndirectBuffer(frame_index)->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::INDIRECT_ARG
    );
}

void IndirectRenderer::RebuildDescriptors(Frame *frame)
{
    const UInt frame_index = frame->GetFrameIndex();

    auto &descriptor_set = m_descriptor_sets[frame_index];
    AssertThrow(descriptor_set.IsValid());

    descriptor_set->GetDescriptor(3)->RemoveSubDescriptor(0);
    descriptor_set->GetDescriptor(3)->SetElementBuffer(0, m_indirect_draw_state.GetInstanceBuffer(frame_index));

    descriptor_set->GetDescriptor(4)->RemoveSubDescriptor(0);
    descriptor_set->GetDescriptor(4)->SetElementBuffer(0, m_indirect_draw_state.GetIndirectBuffer(frame_index));

    descriptor_set->ApplyUpdates(g_engine->GetGPUDevice());
}

} // namespace hyperion::v2