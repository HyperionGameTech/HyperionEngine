/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rendering/GaussianSplatting.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/Renderer.hpp>

#include <rendering/rhi/RHICommandList.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/Mesh.hpp>

#include <scene/camera/OrthoCamera.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Color.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <util/NoiseFactory.hpp>
#include <util/MeshBuilder.hpp>

#include <Engine.hpp>

// #define HYP_GAUSSIAN_SPLATTING_CPU_SORT

namespace hyperion {

using renderer::CommandBufferType;
using renderer::GPUBufferType;
using renderer::IndirectDrawCommand;

enum BitonicSortStage : uint32
{
    STAGE_LOCAL_BMS,
    STAGE_LOCAL_DISPERSE,
    STAGE_BIG_FLIP,
    STAGE_BIG_DISPERSE
};

struct alignas(8) GaussianSplatIndex
{
    uint32 index;
    float32 distance;
};

struct RENDER_COMMAND(CreateGaussianSplattingInstanceBuffers)
    : renderer::RenderCommand
{
    GPUBufferRef splat_buffer;
    GPUBufferRef splat_indices_buffer;
    GPUBufferRef scene_buffer;
    GPUBufferRef indirect_buffer;
    RC<GaussianSplattingModelData> model;

    RENDER_COMMAND(CreateGaussianSplattingInstanceBuffers)(
        GPUBufferRef splat_buffer,
        GPUBufferRef splat_indices_buffer,
        GPUBufferRef scene_buffer,
        GPUBufferRef indirect_buffer,
        RC<GaussianSplattingModelData> model)
        : splat_buffer(std::move(splat_buffer)),
          splat_indices_buffer(std::move(splat_indices_buffer)),
          scene_buffer(std::move(scene_buffer)),
          indirect_buffer(std::move(indirect_buffer)),
          model(std::move(model))
    {
    }

    virtual ~RENDER_COMMAND(CreateGaussianSplattingInstanceBuffers)() override = default;

    virtual RendererResult operator()() override
    {
        static_assert(sizeof(GaussianSplattingInstanceShaderData) == sizeof(model->points[0]));

        const SizeType num_points = model->points.Size();

        HYPERION_BUBBLE_ERRORS(splat_buffer->Create());

        splat_buffer->Copy(splat_buffer->Size(), model->points.Data());

        HYPERION_BUBBLE_ERRORS(splat_indices_buffer->Create());

        // Set default indices
        GaussianSplatIndex* indices_buffer_data = new GaussianSplatIndex[splat_indices_buffer->Size() / sizeof(GaussianSplatIndex)];

        for (SizeType index = 0; index < num_points; index++)
        {
            if (index >= UINT32_MAX)
            {
                break;
            }

            indices_buffer_data[index] = GaussianSplatIndex {
                uint32(index),
                -1000.0f
            };
        }

        for (SizeType index = num_points; index < splat_indices_buffer->Size() / sizeof(GaussianSplatIndex); index++)
        {
            indices_buffer_data[index] = GaussianSplatIndex {
                uint32(-1),
                -1000.0f
            };
        }

        splat_indices_buffer->Copy(splat_indices_buffer->Size(), indices_buffer_data);

        // Discard the data we used for initially setting up the buffers
        delete[] indices_buffer_data;

        const GaussianSplattingSceneShaderData gaussian_splatting_scene_shader_data = {
            model->transform.GetMatrix()
        };

        HYPERION_BUBBLE_ERRORS(scene_buffer->Create());
        scene_buffer->Copy(sizeof(GaussianSplattingSceneShaderData), &gaussian_splatting_scene_shader_data);

        HYPERION_BUBBLE_ERRORS(indirect_buffer->Create());

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers)
    : renderer::RenderCommand
{
    GPUBufferRef staging_buffer;
    Handle<Mesh> quad_mesh;

    RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers)(
        GPUBufferRef staging_buffer,
        Handle<Mesh> quad_mesh)
        : staging_buffer(std::move(staging_buffer)),
          quad_mesh(std::move(quad_mesh))
    {
    }

    virtual ~RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers)() override = default;

    virtual RendererResult operator()() override
    {
        HYPERION_BUBBLE_ERRORS(staging_buffer->Create());

        IndirectDrawCommand empty_draw_command {};
        quad_mesh->GetRenderResource().PopulateIndirectDrawCommand(empty_draw_command);

        // copy zeros to buffer
        staging_buffer->Copy(sizeof(IndirectDrawCommand), &empty_draw_command);

        HYPERION_RETURN_OK;
    }
};

GaussianSplattingInstance::GaussianSplattingInstance()
{
}

GaussianSplattingInstance::GaussianSplattingInstance(RC<GaussianSplattingModelData> model)
    : m_model(std::move(model))
{
}

GaussianSplattingInstance::~GaussianSplattingInstance()
{
    if (IsInitCalled())
    {
        SafeRelease(std::move(m_splat_buffer));
        SafeRelease(std::move(m_splat_indices_buffer));
        SafeRelease(std::move(m_scene_buffer));
        SafeRelease(std::move(m_indirect_buffer));
        SafeRelease(std::move(m_sort_stage_descriptor_tables));
    }
}

void GaussianSplattingInstance::Init()
{
    CreateBuffers();
    CreateShader();
    CreateRenderGroup();
    CreateComputePipelines();

#ifdef HYP_GAUSSIAN_SPLATTING_CPU_SORT
    // Temporary
    m_cpu_sorted_indices.Resize(m_model->points.Size());
    m_cpu_distances.Resize(m_model->points.Size());

    for (SizeType index = 0; index < m_model->points.Size(); index++)
    {
        m_cpu_sorted_indices[index] = index;
        m_cpu_distances[index] = -1000.0f;
    }
#endif

    HYP_SYNC_RENDER();

    SetReady(true);
}

void GaussianSplattingInstance::Record(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(IsReady());

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    const uint32 num_points = static_cast<uint32>(m_model->points.Size());

    AssertThrow(m_splat_buffer->Size() == sizeof(GaussianSplattingInstanceShaderData) * num_points);

    { // Update splat distances from camera before we sort

        struct
        {
            uint32 num_points;
        } update_splats_distances_push_constants;

        update_splats_distances_push_constants.num_points = num_points;

        m_update_splat_distances->SetPushConstants(
            &update_splats_distances_push_constants,
            sizeof(update_splats_distances_push_constants));

        frame->GetCommandList().Add<BindComputePipeline>(m_update_splat_distances);

        frame->GetCommandList().Add<BindDescriptorTable>(
            m_update_splat_distances->GetDescriptorTable(),
            m_update_splat_distances,
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                { NAME("Global"),
                    { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                        { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) } } } },
            frame->GetFrameIndex());

        frame->GetCommandList().Add<DispatchCompute>(
            m_update_splat_distances,
            Vec3u { uint32((num_points + 255) / 256), 1, 1 });

        frame->GetCommandList().Add<InsertBarrier>(
            m_splat_indices_buffer,
            renderer::ResourceState::UNORDERED_ACCESS);
    }

#ifdef HYP_GAUSSIAN_SPLATTING_CPU_SORT
    { // Temporary CPU sorting -- inefficient but useful for testing
        for (SizeType index = 0; index < m_model->points.Size(); index++)
        {
            m_cpu_distances[index] = ((active_camera ? active_camera->GetBufferData().view : Matrix4()) * m_model->points[index].position).z;
        }

        std::sort(m_cpu_sorted_indices.Begin(), m_cpu_sorted_indices.End(), [&distances = m_cpu_distances](uint32 a, uint32 b)
            {
                return distances[a] < distances[b];
            });

        AssertThrowMsg(m_splat_indices_buffer->Size() >= m_cpu_sorted_indices.Size() * sizeof(m_cpu_sorted_indices[0]),
            "Expected buffer size to be at least %llu -- got %llu.",
            m_cpu_sorted_indices.Size() * sizeof(m_cpu_sorted_indices[0]),
            m_splat_indices_buffer->Size());

        // Copy the cpu sorted indices over
        m_splat_indices_buffer->Copy(MathUtil::Min(m_splat_indices_buffer->Size(), m_cpu_sorted_indices.Size() * sizeof(m_cpu_sorted_indices[0])), m_cpu_sorted_indices.Data());
    }
#else
    { // Sort splats
        constexpr uint32 block_size = 512;
        constexpr uint32 transpose_block_size = 16;

        struct
        {
            uint32 num_points;
            uint32 stage;
            uint32 h;
        } sort_splats_push_constants;

        Memory::MemSet(&sort_splats_push_constants, 0x0, sizeof(sort_splats_push_constants));

        const uint32 num_sortable_elements = uint32(MathUtil::NextPowerOf2(num_points)); // Values are stored in components of uvec4

        const uint32 width = block_size;
        const uint32 height = num_sortable_elements / block_size;

        sort_splats_push_constants.num_points = num_points;

        frame->GetCommandList().Add<InsertBarrier>(
            m_splat_indices_buffer,
            renderer::ResourceState::UNORDERED_ACCESS);

        static constexpr uint32 max_workgroup_size = 512;
        uint32 workgroup_size_x = 1;

        if (num_sortable_elements < max_workgroup_size * 2)
        {
            workgroup_size_x = num_sortable_elements / 2;
        }
        else
        {
            workgroup_size_x = max_workgroup_size;
        }

        AssertThrowMsg(workgroup_size_x == max_workgroup_size, "Not implemented for workgroup size < max_workgroup_size");

        uint32 h = workgroup_size_x * 2;
        const uint32 workgroup_count = num_sortable_elements / (workgroup_size_x * 2);

        AssertThrow(h < num_sortable_elements);
        AssertThrow(h % 2 == 0);

        auto do_pass = [this, frame, &render_setup, pc = sort_splats_push_constants, workgroup_count](BitonicSortStage stage, uint32 h) mutable
        {
            pc.stage = uint32(stage);
            pc.h = h;

            m_sort_splats->SetPushConstants(&pc, sizeof(pc));

            frame->GetCommandList().Add<BindComputePipeline>(m_sort_splats);

            frame->GetCommandList().Add<BindDescriptorTable>(
                m_sort_splats->GetDescriptorTable(),
                m_sort_splats,
                ArrayMap<Name, ArrayMap<Name, uint32>> {
                    { NAME("Global"),
                        { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                            { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) } } } },
                frame->GetFrameIndex());

            frame->GetCommandList().Add<DispatchCompute>(
                m_sort_splats,
                Vec3u { workgroup_count, 1, 1 });

            frame->GetCommandList().Add<InsertBarrier>(
                m_splat_indices_buffer,
                renderer::ResourceState::UNORDERED_ACCESS);
        };

        do_pass(STAGE_LOCAL_BMS, h);

        h <<= 1;

        for (; h <= num_sortable_elements; h <<= 1)
        {
            do_pass(STAGE_BIG_FLIP, h);

            for (uint32 hh = h >> 1; hh > 1; hh >>= 1)
            {
                if (hh <= workgroup_size_x * 2)
                {
                    do_pass(STAGE_LOCAL_DISPERSE, hh);

                    break;
                }
                else
                {
                    do_pass(STAGE_BIG_DISPERSE, hh);
                }
            }
        }
    }
#endif
    { // Update splats

        struct
        {
            uint32 num_points;
        } update_splats_push_constants;

        update_splats_push_constants.num_points = num_points;

        m_update_splats->SetPushConstants(
            &update_splats_push_constants,
            sizeof(update_splats_push_constants));

        frame->GetCommandList().Add<BindComputePipeline>(m_update_splats);

        frame->GetCommandList().Add<BindDescriptorTable>(
            m_update_splats->GetDescriptorTable(),
            m_update_splats,
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                { NAME("Global"),
                    { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                        { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) } } } },
            frame->GetFrameIndex());

        frame->GetCommandList().Add<DispatchCompute>(
            m_update_splats,
            Vec3u { uint32((num_points + 255) / 256), 1, 1 });

        frame->GetCommandList().Add<InsertBarrier>(
            m_indirect_buffer,
            renderer::ResourceState::INDIRECT_ARG);
    }
}

void GaussianSplattingInstance::CreateBuffers()
{
    const SizeType num_points = m_model->points.Size();

    m_splat_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::STORAGE_BUFFER, num_points * sizeof(GaussianSplattingInstanceShaderData));
    m_splat_indices_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::STORAGE_BUFFER, MathUtil::NextPowerOf2(num_points) * sizeof(GaussianSplatIndex));
    m_scene_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::CONSTANT_BUFFER, sizeof(GaussianSplattingSceneShaderData));
    m_indirect_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::INDIRECT_ARGS_BUFFER, sizeof(IndirectDrawCommand));

    PUSH_RENDER_COMMAND(
        CreateGaussianSplattingInstanceBuffers,
        m_splat_buffer,
        m_splat_indices_buffer,
        m_scene_buffer,
        m_indirect_buffer,
        m_model);
}

void GaussianSplattingInstance::CreateShader()
{
    m_shader = g_shader_manager->GetOrCreate(NAME("GaussianSplatting"));
}

void GaussianSplattingInstance::CreateRenderGroup()
{
    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&m_shader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("GaussianSplattingDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("SplatIndicesBuffer"), m_splat_indices_buffer);
        descriptor_set->SetElement(NAME("SplatInstancesBuffer"), m_splat_buffer);
        descriptor_set->SetElement(NAME("GaussianSplattingSceneShaderData"), m_scene_buffer);
    }

    DeferCreate(descriptor_table);

    m_render_group = CreateObject<RenderGroup>(
        m_shader,
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = static_mesh_vertex_attributes // VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION
            },
            MaterialAttributes {
                .bucket = Bucket::BUCKET_TRANSLUCENT,
                .blend_function = BlendFunction::Additive(),
                .cull_faces = FaceCullMode::NONE,
                .flags = MaterialAttributeFlags::NONE }),
        descriptor_table,
        RenderGroupFlags::NONE);

    AssertThrow(InitObject(m_render_group));
}

void GaussianSplattingInstance::CreateComputePipelines()
{
    ShaderProperties base_properties;

    // UpdateSplats

    ShaderRef update_splats_shader = g_shader_manager->GetOrCreate(
        NAME("GaussianSplatting_UpdateSplats"),
        base_properties);

    DescriptorTableRef update_splats_descriptor_table = g_rendering_api->MakeDescriptorTable(&update_splats_shader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = update_splats_descriptor_table->GetDescriptorSet(NAME("UpdateSplatsDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("SplatIndicesBuffer"), m_splat_indices_buffer);
        descriptor_set->SetElement(NAME("SplatInstancesBuffer"), m_splat_buffer);
        descriptor_set->SetElement(NAME("IndirectDrawCommandsBuffer"), m_indirect_buffer);
        descriptor_set->SetElement(NAME("GaussianSplattingSceneShaderData"), m_scene_buffer);
    }

    DeferCreate(update_splats_descriptor_table);

    m_update_splats = g_rendering_api->MakeComputePipeline(
        update_splats_shader,
        update_splats_descriptor_table);

    DeferCreate(m_update_splats);

    // UpdateDistances

    ShaderRef update_splat_distances_shader = g_shader_manager->GetOrCreate(
        NAME("GaussianSplatting_UpdateDistances"),
        base_properties);

    DescriptorTableRef update_splat_distances_descriptor_table = g_rendering_api->MakeDescriptorTable(&update_splat_distances_shader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = update_splat_distances_descriptor_table->GetDescriptorSet(NAME("UpdateDistancesDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("SplatIndicesBuffer"), m_splat_indices_buffer);
        descriptor_set->SetElement(NAME("SplatInstancesBuffer"), m_splat_buffer);
        descriptor_set->SetElement(NAME("GaussianSplattingSceneShaderData"), m_scene_buffer);
    }

    DeferCreate(update_splat_distances_descriptor_table);

    m_update_splat_distances = g_rendering_api->MakeComputePipeline(
        update_splat_distances_shader,
        update_splat_distances_descriptor_table);

    DeferCreate(m_update_splat_distances);

    // SortSplats

    ShaderRef sort_splats_shader = g_shader_manager->GetOrCreate(
        NAME("GaussianSplatting_SortSplats"),
        base_properties);

    m_sort_stage_descriptor_tables.Resize(SortStage::SORT_STAGE_MAX);

    for (uint32 sort_stage_index = 0; sort_stage_index < SortStage::SORT_STAGE_MAX; sort_stage_index++)
    {
        DescriptorTableRef sort_splats_descriptor_table = g_rendering_api->MakeDescriptorTable(&sort_splats_shader->GetCompiledShader()->GetDescriptorTableDeclaration());

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            const DescriptorSetRef& descriptor_set = sort_splats_descriptor_table->GetDescriptorSet(NAME("SortSplatsDescriptorSet"), frame_index);
            AssertThrow(descriptor_set != nullptr);

            descriptor_set->SetElement(NAME("SplatIndicesBuffer"), m_splat_indices_buffer);
            descriptor_set->SetElement(NAME("SplatInstancesBuffer"), m_splat_buffer);
            descriptor_set->SetElement(NAME("GaussianSplattingSceneShaderData"), m_scene_buffer);
        }

        DeferCreate(sort_splats_descriptor_table);

        m_sort_stage_descriptor_tables[sort_stage_index] = std::move(sort_splats_descriptor_table);
    }

    m_sort_splats = g_rendering_api->MakeComputePipeline(
        sort_splats_shader,
        m_sort_stage_descriptor_tables[0]);

    DeferCreate(m_sort_splats);
}

GaussianSplatting::GaussianSplatting()
    : HypObject()
{
}

GaussianSplatting::~GaussianSplatting()
{
    if (IsInitCalled())
    {
        m_quad_mesh.Reset();
        m_gaussian_splatting_instance.Reset();

        SafeRelease(std::move(m_staging_buffer));
    }
}

void GaussianSplatting::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
        {
            m_quad_mesh.Reset();
            m_gaussian_splatting_instance.Reset();

            SafeRelease(std::move(m_staging_buffer));
        }));

    static const Array<Vertex> vertices = {
        Vertex { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
        Vertex { { 1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
        Vertex { { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
        Vertex { { -1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } }
    };

    static const Array<Mesh::Index> indices = {
        0, 3, 1,
        2, 3, 1
    };

    m_quad_mesh = CreateObject<Mesh>(
        vertices,
        indices,
        renderer::Topology::TRIANGLES,
        static_mesh_vertex_attributes);

    InitObject(m_quad_mesh);

    InitObject(m_gaussian_splatting_instance);

    CreateBuffers();

    SetReady(true);
}

void GaussianSplatting::SetGaussianSplattingInstance(Handle<GaussianSplattingInstance> gaussian_splatting_instance)
{
    m_gaussian_splatting_instance = std::move(gaussian_splatting_instance);

    if (IsInitCalled())
    {
        InitObject(m_gaussian_splatting_instance);
    }
}

void GaussianSplatting::CreateBuffers()
{
    m_staging_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::STAGING_BUFFER, sizeof(IndirectDrawCommand));

    PUSH_RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers,
        m_staging_buffer,
        m_quad_mesh);
}

void GaussianSplatting::UpdateSplats(FrameBase* frame, const RenderSetup& render_setup)
{
    Threads::AssertOnThread(g_render_thread);
    AssertReady();

    if (!m_gaussian_splatting_instance)
    {
        return;
    }

    AssertThrow(m_gaussian_splatting_instance->GetIndirectBuffer()->Size() == sizeof(IndirectDrawCommand));

    frame->GetCommandList().Add<InsertBarrier>(
        m_staging_buffer,
        renderer::ResourceState::COPY_SRC);

    frame->GetCommandList().Add<InsertBarrier>(
        m_gaussian_splatting_instance->GetIndirectBuffer(),
        renderer::ResourceState::COPY_DST);

    frame->GetCommandList().Add<CopyBuffer>(
        m_staging_buffer,
        m_gaussian_splatting_instance->GetIndirectBuffer(),
        sizeof(IndirectDrawCommand));

    frame->GetCommandList().Add<InsertBarrier>(
        m_gaussian_splatting_instance->GetIndirectBuffer(),
        renderer::ResourceState::INDIRECT_ARG);

    m_gaussian_splatting_instance->Record(frame, render_setup);
}

void GaussianSplatting::Render(FrameBase* frame, const RenderSetup& render_setup)
{
    AssertReady();
    Threads::AssertOnThread(g_render_thread);

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    if (!m_gaussian_splatting_instance)
    {
        return;
    }

    const uint32 frame_index = frame->GetFrameIndex();

    const GraphicsPipelineRef& pipeline = m_gaussian_splatting_instance->GetRenderGroup()->GetPipeline();

    frame->GetCommandList().Add<BindGraphicsPipeline>(pipeline);

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_gaussian_splatting_instance->GetRenderGroup()->GetPipeline()->GetDescriptorTable(),
        pipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) } } } },
        frame_index);

    m_quad_mesh->GetRenderResource().RenderIndirect(
        frame->GetCommandList(),
        m_gaussian_splatting_instance->GetIndirectBuffer());
}

} // namespace hyperion
