/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/IndirectDraw.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderState.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <scene/Mesh.hpp>

#include <core/math/MathUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

static bool ResizeBuffer(
    FrameBase* frame,
    const GPUBufferRef& buffer,
    SizeType new_buffer_size)
{
    if constexpr (IndirectDrawState::use_next_pow2_size)
    {
        new_buffer_size = MathUtil::NextPowerOf2(new_buffer_size);
    }

    bool size_changed = false;

    HYPERION_ASSERT_RESULT(buffer->EnsureCapacity(new_buffer_size, &size_changed));

    return size_changed;
}

static bool ResizeIndirectDrawCommandsBuffer(
    FrameBase* frame,
    uint32 num_draw_commands,
    const GPUBufferRef& indirect_buffer,
    const GPUBufferRef& staging_buffer)
{
    const bool was_created_or_resized = ResizeBuffer(
        frame,
        indirect_buffer,
        num_draw_commands * sizeof(IndirectDrawCommand));

    if (!was_created_or_resized)
    {
        return false;
    }

    // upload zeros to the buffer using a staging buffer.
    if (!staging_buffer->IsCreated())
    {
        HYPERION_ASSERT_RESULT(staging_buffer->Create());
    }

    HYPERION_ASSERT_RESULT(staging_buffer->EnsureCapacity(indirect_buffer->Size()));

    staging_buffer->Memset(staging_buffer->Size(), 0);

    frame->GetCommandList().Add<InsertBarrier>(
        staging_buffer,
        renderer::ResourceState::COPY_SRC);

    frame->GetCommandList().Add<InsertBarrier>(
        indirect_buffer,
        renderer::ResourceState::COPY_DST);

    frame->GetCommandList().Add<CopyBuffer>(
        staging_buffer,
        indirect_buffer,
        staging_buffer->Size());

    frame->GetCommandList().Add<InsertBarrier>(
        indirect_buffer,
        renderer::ResourceState::INDIRECT_ARG);

    return true;
}

static bool ResizeInstancesBuffer(
    FrameBase* frame,
    uint32 num_object_instances,
    const GPUBufferRef& instance_buffer,
    const GPUBufferRef& staging_buffer)
{
    const bool was_created_or_resized = ResizeBuffer(
        frame,
        instance_buffer,
        num_object_instances * sizeof(ObjectInstance));

    if (was_created_or_resized)
    {
        instance_buffer->Memset(instance_buffer->Size(), 0);
    }

    return was_created_or_resized;
}

static bool ResizeIfNeeded(
    FrameBase* frame,
    const FixedArray<GPUBufferRef, max_frames_in_flight>& indirect_buffers,
    const FixedArray<GPUBufferRef, max_frames_in_flight>& instance_buffers,
    const FixedArray<GPUBufferRef, max_frames_in_flight>& staging_buffers,
    uint32 num_draw_commands,
    uint32 num_object_instances,
    uint8 dirty_bits)
{
    bool resize_happened = false;

    const GPUBufferRef& indirect_buffer = indirect_buffers[frame->GetFrameIndex()];
    const GPUBufferRef& instance_buffer = instance_buffers[frame->GetFrameIndex()];
    const GPUBufferRef& staging_buffer = staging_buffers[frame->GetFrameIndex()];

    if ((dirty_bits & (1u << frame->GetFrameIndex())) || !indirect_buffers[frame->GetFrameIndex()].IsValid())
    {
        resize_happened |= ResizeIndirectDrawCommandsBuffer(frame, num_draw_commands, indirect_buffer, staging_buffer);
    }

    if ((dirty_bits & (1u << frame->GetFrameIndex())) || !instance_buffers[frame->GetFrameIndex()].IsValid())
    {
        resize_happened |= ResizeInstancesBuffer(frame, num_object_instances, instance_buffer, staging_buffer);
    }

    return resize_happened;
}

#pragma region Render commands

struct RENDER_COMMAND(CreateIndirectDrawStateBuffers)
    : renderer::RenderCommand
{
    FixedArray<GPUBufferRef, max_frames_in_flight> indirect_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight> instance_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight> staging_buffers;

    RENDER_COMMAND(CreateIndirectDrawStateBuffers)(
        const FixedArray<GPUBufferRef, max_frames_in_flight>& indirect_buffers,
        const FixedArray<GPUBufferRef, max_frames_in_flight>& instance_buffers,
        const FixedArray<GPUBufferRef, max_frames_in_flight>& staging_buffers)
        : indirect_buffers(indirect_buffers),
          instance_buffers(instance_buffers),
          staging_buffers(staging_buffers)
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            AssertThrow(this->indirect_buffers[frame_index].IsValid());
            AssertThrow(this->instance_buffers[frame_index].IsValid());
            AssertThrow(this->staging_buffers[frame_index].IsValid());
        }
    }

    virtual ~RENDER_COMMAND(CreateIndirectDrawStateBuffers)() override
    {
        SafeRelease(std::move(indirect_buffers));
        SafeRelease(std::move(instance_buffers));
        SafeRelease(std::move(staging_buffers));
    }

    virtual RendererResult operator()() override
    {
        renderer::SingleTimeCommands commands;

        commands.Push([this](RHICommandList& cmd)
            {
                for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
                {
                    FrameRef frame = g_rendering_api->MakeFrame(frame_index);

                    if (!ResizeIndirectDrawCommandsBuffer(frame, IndirectDrawState::initial_count, indirect_buffers[frame_index], staging_buffers[frame_index]))
                    {
                        HYP_FAIL("Failed to create indirect draw commands buffer!");
                    }

                    if (!ResizeInstancesBuffer(frame, IndirectDrawState::initial_count, instance_buffers[frame_index], staging_buffers[frame_index]))
                    {
                        HYP_FAIL("Failed to create instances buffer!");
                    }

                    cmd.Concat(std::move(frame->GetCommandList()));

                    HYPERION_ASSERT_RESULT(frame->Destroy());
                }
            });

        return commands.Execute();
    }
};

#pragma endregion Render commands

#pragma region IndirectDrawState

IndirectDrawState::IndirectDrawState()
    : m_num_draw_commands(0),
      m_dirty_bits(0x3)
{
    // Allocate used buffers so they can be set in descriptor sets
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        m_indirect_buffers[frame_index] = g_rendering_api->MakeGPUBuffer(renderer::GPUBufferType::INDIRECT_ARGS_BUFFER, sizeof(IndirectDrawCommand));
        m_instance_buffers[frame_index] = g_rendering_api->MakeGPUBuffer(renderer::GPUBufferType::STORAGE_BUFFER, sizeof(ObjectInstance));
        m_staging_buffers[frame_index] = g_rendering_api->MakeGPUBuffer(renderer::GPUBufferType::STAGING_BUFFER, sizeof(IndirectDrawCommand));
    }
}

IndirectDrawState::~IndirectDrawState()
{
}

void IndirectDrawState::Create()
{
    PUSH_RENDER_COMMAND(
        CreateIndirectDrawStateBuffers,
        m_indirect_buffers,
        m_instance_buffers,
        m_staging_buffers);
}

void IndirectDrawState::Destroy()
{
    SafeRelease(std::move(m_indirect_buffers));
    SafeRelease(std::move(m_instance_buffers));
    SafeRelease(std::move(m_staging_buffers));
}

void IndirectDrawState::PushDrawCall(const DrawCall& draw_call, DrawCommandData& out)
{
    // assume render thread or render task thread

    out = {};

    const uint32 draw_command_index = m_num_draw_commands++;

    for (uint32 index = 0; index < draw_call.entity_id_count; index++)
    {
        m_object_instances.EmplaceBack(ObjectInstance {
            draw_call.entity_ids[index].Value(),
            draw_command_index,
            index,
            draw_call.batch->batch_index });
    }

    out.draw_command_index = draw_command_index;

    if (m_draw_commands.Size() < m_num_draw_commands)
    {
        m_draw_commands.Resize(m_num_draw_commands);
    }

    draw_call.render_mesh->PopulateIndirectDrawCommand(m_draw_commands[draw_command_index]);

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

void IndirectDrawState::UpdateBufferData(FrameBase* frame, bool* out_was_resized)
{
    // assume render thread

    const uint32 frame_index = frame->GetFrameIndex();

    if ((*out_was_resized = ResizeIfNeeded(
             frame,
             m_indirect_buffers,
             m_instance_buffers,
             m_staging_buffers,
             m_num_draw_commands,
             m_object_instances.Size(),
             m_dirty_bits)))
    {
        m_dirty_bits |= (1u << frame_index);
    }

    if (!(m_dirty_bits & (1u << frame_index)))
    {
        return;
    }

    // fill instances buffer with data of the meshes
    {
        AssertThrow(m_staging_buffers[frame_index].IsValid());
        AssertThrow(m_staging_buffers[frame_index]->Size() >= sizeof(IndirectDrawCommand) * m_draw_commands.Size());

        m_staging_buffers[frame_index]->Copy(
            m_draw_commands.Size() * sizeof(IndirectDrawCommand),
            m_draw_commands.Data());

        frame->GetCommandList().Add<InsertBarrier>(
            m_staging_buffers[frame_index],
            renderer::ResourceState::COPY_SRC);

        frame->GetCommandList().Add<InsertBarrier>(
            m_indirect_buffers[frame_index],
            renderer::ResourceState::COPY_DST);

        frame->GetCommandList().Add<CopyBuffer>(
            m_staging_buffers[frame_index],
            m_indirect_buffers[frame_index],
            m_staging_buffers[frame_index]->Size());

        frame->GetCommandList().Add<InsertBarrier>(
            m_indirect_buffers[frame_index],
            renderer::ResourceState::INDIRECT_ARG);
    }

    AssertThrow(m_instance_buffers[frame_index]->Size() >= m_object_instances.Size() * sizeof(ObjectInstance));

    // update data for object instances (cpu - gpu)
    m_instance_buffers[frame_index]->Copy(
        m_object_instances.Size() * sizeof(ObjectInstance),
        m_object_instances.Data());

    m_dirty_bits &= ~(1u << frame_index);
}

#pragma endregion IndirectDrawState

#pragma region IndirectRenderer

IndirectRenderer::IndirectRenderer(DrawCallCollection* draw_call_collection)
    : m_draw_call_collection(draw_call_collection),
      m_cached_cull_data_updated_bits(0x0)
{
    AssertThrow(m_draw_call_collection != nullptr);
}

IndirectRenderer::~IndirectRenderer() = default;

void IndirectRenderer::Create()
{
    m_indirect_draw_state.Create();

    ShaderRef object_visibility_shader = g_shader_manager->GetOrCreate(NAME("ObjectVisibility"));
    AssertThrow(object_visibility_shader.IsValid());

    const renderer::DescriptorTableDeclaration& descriptor_table_decl = object_visibility_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

    AssertThrow(m_draw_call_collection != nullptr);
    AssertThrow(m_draw_call_collection->GetImpl() != nullptr);

    GPUBufferHolderBase* entity_instance_batches = m_draw_call_collection->GetImpl()->GetEntityInstanceBatchHolder();
    const SizeType batch_sizeof = m_draw_call_collection->GetImpl()->GetBatchSizeOf();

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("ObjectVisibilityDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        auto* entity_instance_batches_buffer_element = descriptor_set->GetLayout().GetElement(NAME("EntityInstanceBatchesBuffer"));
        AssertThrow(entity_instance_batches_buffer_element != nullptr);

        if (entity_instance_batches_buffer_element->size != ~0u)
        {
            const SizeType entity_instance_batches_buffer_size = entity_instance_batches_buffer_element->size;
            const SizeType size_mod = entity_instance_batches_buffer_size % batch_sizeof;

            AssertThrowMsg(size_mod == 0,
                "EntityInstanceBatchesBuffer descriptor has size %llu but DrawCallCollection has batch struct size of %llu",
                entity_instance_batches_buffer_size,
                batch_sizeof);
        }

        descriptor_set->SetElement(NAME("ObjectInstancesBuffer"), m_indirect_draw_state.GetInstanceBuffer(frame_index));
        descriptor_set->SetElement(NAME("IndirectDrawCommandsBuffer"), m_indirect_draw_state.GetIndirectBuffer(frame_index));
        descriptor_set->SetElement(NAME("EntityInstanceBatchesBuffer"), entity_instance_batches->GetBuffer(frame_index));
    }

    DeferCreate(descriptor_table);

    m_object_visibility = g_rendering_api->MakeComputePipeline(
        object_visibility_shader,
        descriptor_table);

    // use DeferCreate because our Shader might not be ready yet
    DeferCreate(m_object_visibility);
}

void IndirectRenderer::Destroy()
{
    SafeRelease(std::move(m_object_visibility));

    m_indirect_draw_state.Destroy();
}

void IndirectRenderer::PushDrawCallsToIndirectState()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    for (DrawCall& draw_call : m_draw_call_collection->GetDrawCalls())
    {
        DrawCommandData draw_command_data;
        m_indirect_draw_state.PushDrawCall(draw_call, draw_command_data);

        draw_call.draw_command_index = draw_command_data.draw_command_index;
    }
}

void IndirectRenderer::ExecuteCullShaderInBatches(FrameBase* frame, const RenderSetup& render_setup, const CullData& cull_data)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    const TResourceHandle<RenderEnvProbe>& env_render_probe = g_engine->GetRenderState()->GetActiveEnvProbe();
    const TResourceHandle<RenderEnvGrid>& env_render_grid = g_engine->GetRenderState()->GetActiveEnvGrid();

    AssertThrow(m_indirect_draw_state.GetIndirectBuffer(frame_index).IsValid());
    AssertThrow(m_indirect_draw_state.GetIndirectBuffer(frame_index)->Size() != 0);

    const SizeType num_instances = m_indirect_draw_state.GetInstances().Size();
    const uint32 num_batches = (uint32(num_instances) / IndirectDrawState::batch_size) + 1;

    if (num_instances == 0)
    {
        return;
    }

    {
        bool was_buffer_resized = false;
        m_indirect_draw_state.UpdateBufferData(frame, &was_buffer_resized);

        if (was_buffer_resized)
        {
            RebuildDescriptors(frame);
        }
    }

    if (m_cached_cull_data != cull_data)
    {
        m_cached_cull_data = cull_data;
        m_cached_cull_data_updated_bits = 0xFF;
    }

    // if (m_cached_cull_data_updated_bits & (1u << frame_index)) {
    //     m_descriptor_sets[frame_index]->GetDescriptor(6)
    //         ->SetElementSRV(0, m_cached_cull_data.depth_pyramid_image_view);

    //     m_descriptor_sets[frame_index]->ApplyUpdates();

    //     m_cached_cull_data_updated_bits &= ~(1u << frame_index);
    // }

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_object_visibility->GetDescriptorTable(),
        m_object_visibility,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_render_grid.Get(), 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_render_probe.Get(), 0) } } } },
        frame_index);

    const uint32 scene_descriptor_set_index = m_object_visibility->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));

    if (render_setup.HasView() && scene_descriptor_set_index != ~0u)
    {
        frame->GetCommandList().Add<BindDescriptorSet>(
            render_setup.view->GetDescriptorSets()[frame_index],
            m_object_visibility,
            ArrayMap<Name, uint32> {},
            scene_descriptor_set_index);
    }

    frame->GetCommandList().Add<InsertBarrier>(
        m_indirect_draw_state.GetIndirectBuffer(frame_index),
        renderer::ResourceState::INDIRECT_ARG);

    struct
    {
        uint32 batch_offset;
        uint32 num_instances;
        Vec2u depth_pyramid_dimensions;
    } push_constants;

    push_constants.batch_offset = 0;
    push_constants.num_instances = num_instances;
    push_constants.depth_pyramid_dimensions = render_setup.view->GetDepthPyramidRenderer()->GetExtent();

    m_object_visibility->SetPushConstants(&push_constants, sizeof(push_constants));

    frame->GetCommandList().Add<BindComputePipeline>(m_object_visibility);

    frame->GetCommandList().Add<DispatchCompute>(
        m_object_visibility,
        Vec3u { num_batches, 1, 1 });

    frame->GetCommandList().Add<InsertBarrier>(
        m_indirect_draw_state.GetIndirectBuffer(frame_index),
        renderer::ResourceState::INDIRECT_ARG);
}

void IndirectRenderer::RebuildDescriptors(FrameBase* frame)
{
    HYP_SCOPE;

    const uint32 frame_index = frame->GetFrameIndex();

    const DescriptorTableRef& descriptor_table = m_object_visibility->GetDescriptorTable();

    const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("ObjectVisibilityDescriptorSet"), frame_index);
    AssertThrow(descriptor_set != nullptr);

    descriptor_set->SetElement(NAME("ObjectInstancesBuffer"), m_indirect_draw_state.GetInstanceBuffer(frame_index));
    descriptor_set->SetElement(NAME("IndirectDrawCommandsBuffer"), m_indirect_draw_state.GetIndirectBuffer(frame_index));

    descriptor_set->Update();
}

#pragma endregion IndirectRenderer

} // namespace hyperion