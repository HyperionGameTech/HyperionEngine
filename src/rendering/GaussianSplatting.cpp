/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rendering/GaussianSplatting.hpp>

#include <scene/camera/OrthoCamera.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderableAttributes.hpp>

#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <math/MathUtil.hpp>
#include <math/Color.hpp>

#include <util/fs/FsUtil.hpp>
#include <util/NoiseFactory.hpp>
#include <util/MeshBuilder.hpp>

#include <Engine.hpp>

//#define HYP_GAUSSIAN_SPLATTING_CPU_SORT

namespace hyperion {

using renderer::IndirectDrawCommand;
using renderer::Pipeline;
using renderer::Result;
using renderer::GPUBufferType;
using renderer::CommandBufferType;

enum BitonicSortStage : uint32
{
    STAGE_LOCAL_BMS,
    STAGE_LOCAL_DISPERSE,
    STAGE_BIG_FLIP,
    STAGE_BIG_DISPERSE
};

struct alignas(8) GaussianSplatIndex
{
    uint32  index;
    float32 distance;
};

struct RENDER_COMMAND(CreateGaussianSplattingInstanceBuffers) : renderer::RenderCommand
{
    GPUBufferRef                    splat_buffer;
    GPUBufferRef                    splat_indices_buffer;
    GPUBufferRef                    scene_buffer;
    GPUBufferRef                    indirect_buffer;
    RC<GaussianSplattingModelData>  model;

    RENDER_COMMAND(CreateGaussianSplattingInstanceBuffers)(
        GPUBufferRef splat_buffer,
        GPUBufferRef splat_indices_buffer,
        GPUBufferRef scene_buffer,
        GPUBufferRef indirect_buffer,
        RC<GaussianSplattingModelData> model
    ) : splat_buffer(std::move(splat_buffer)),
        splat_indices_buffer(std::move(splat_indices_buffer)),
        scene_buffer(std::move(scene_buffer)),
        indirect_buffer(std::move(indirect_buffer)),
        model(std::move(model))
    {
    }

    virtual ~RENDER_COMMAND(CreateGaussianSplattingInstanceBuffers)() override = default;

    virtual Result operator()() override
    {
        static_assert(sizeof(GaussianSplattingInstanceShaderData) == sizeof(model->points[0]));

        const SizeType num_points = model->points.Size();

        HYPERION_BUBBLE_ERRORS(splat_buffer->Create(
           g_engine->GetGPUDevice(),
           num_points * sizeof(GaussianSplattingInstanceShaderData)
        ));

        splat_buffer->Copy(
            g_engine->GetGPUDevice(),
            splat_buffer->Size(),
            model->points.Data()
        );

        const SizeType indices_buffer_size = MathUtil::NextPowerOf2(num_points) * sizeof(GaussianSplatIndex);
        const SizeType distances_buffer_size = ByteUtil::AlignAs(num_points * sizeof(float32), sizeof(ShaderVec4<float32>));

        HYPERION_BUBBLE_ERRORS(splat_indices_buffer->Create(
           g_engine->GetGPUDevice(),
           indices_buffer_size
        ));

        // Set default indices
        GaussianSplatIndex *indices_buffer_data = new GaussianSplatIndex[indices_buffer_size / sizeof(GaussianSplatIndex)];

        for (SizeType index = 0; index < num_points; index++) {
            if (index >= UINT32_MAX) {
                break;
            }

            indices_buffer_data[index] = GaussianSplatIndex {
                uint32(index),
                -1000.0f
            };
        }

        for (SizeType index = num_points; index < indices_buffer_size / sizeof(GaussianSplatIndex); index++) {
            indices_buffer_data[index] = GaussianSplatIndex {
                uint32(-1),
                -1000.0f
            };
        }

        splat_indices_buffer->Copy(
            g_engine->GetGPUDevice(),
            splat_indices_buffer->Size(),
            indices_buffer_data
        );

        // Discard the data we used for initially setting up the buffers
        delete[] indices_buffer_data;


        const GaussianSplattingSceneShaderData gaussian_splatting_scene_shader_data = {
            model->transform.GetMatrix()
        };

        HYPERION_BUBBLE_ERRORS(scene_buffer->Create(
           g_engine->GetGPUDevice(),
           sizeof(GaussianSplattingSceneShaderData)
        ));

        scene_buffer->Copy(
            g_engine->GetGPUDevice(),
            sizeof(GaussianSplattingSceneShaderData),
            &gaussian_splatting_scene_shader_data
        );

        HYPERION_BUBBLE_ERRORS(indirect_buffer->Create(
            g_engine->GetGPUDevice(),
            sizeof(IndirectDrawCommand)
        ));

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers) : renderer::RenderCommand
{
    GPUBufferRef    staging_buffer;
    Handle<Mesh>    quad_mesh;

    RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers)(
        GPUBufferRef staging_buffer,
        Handle<Mesh> quad_mesh
    ) : staging_buffer(std::move(staging_buffer)),
        quad_mesh(std::move(quad_mesh))
    {
    }

    virtual ~RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers)() override = default;

    virtual Result operator()() override
    {
        HYPERION_BUBBLE_ERRORS(staging_buffer->Create(
            g_engine->GetGPUDevice(),
            sizeof(IndirectDrawCommand)
        ));

        IndirectDrawCommand empty_draw_command { };
        quad_mesh->PopulateIndirectDrawCommand(empty_draw_command);

        // copy zeros to buffer
        staging_buffer->Copy(
            g_engine->GetGPUDevice(),
            sizeof(IndirectDrawCommand),
            &empty_draw_command
        );

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateGaussianSplattingCommandBuffers) : renderer::RenderCommand
{
    FixedArray<CommandBufferRef, max_frames_in_flight> command_buffers;

    RENDER_COMMAND(CreateGaussianSplattingCommandBuffers)(
        FixedArray<CommandBufferRef, max_frames_in_flight> command_buffers
    ) : command_buffers(command_buffers)
    {
    }

    virtual ~RENDER_COMMAND(CreateGaussianSplattingCommandBuffers)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
#ifdef HYP_VULKAN
            command_buffers[frame_index]->GetPlatformImpl().command_pool = g_engine->GetGPUDevice()->GetGraphicsQueue().command_pools[0];
#endif

            HYPERION_BUBBLE_ERRORS(command_buffers[frame_index]->Create(
                g_engine->GetGPUInstance()->GetDevice()
            ));
        }

        HYPERION_RETURN_OK;
    }
};

GaussianSplattingInstance::GaussianSplattingInstance(RC<GaussianSplattingModelData> model)
    : m_model(std::move(model))
{
}

GaussianSplattingInstance::~GaussianSplattingInstance()
{
    if (IsInitCalled()) {
        SafeRelease(std::move(m_splat_buffer));
        SafeRelease(std::move(m_splat_indices_buffer));
        SafeRelease(std::move(m_scene_buffer));
        SafeRelease(std::move(m_indirect_buffer));
        SafeRelease(std::move(m_sort_stage_descriptor_tables));
    }
}

void GaussianSplattingInstance::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    CreateBuffers();
    CreateShader();
    CreateRenderGroup();
    CreateComputePipelines();

#ifdef HYP_GAUSSIAN_SPLATTING_CPU_SORT
    // Temporary
    m_cpu_sorted_indices.Resize(m_model->points.Size());
    m_cpu_distances.Resize(m_model->points.Size());

    for (SizeType index = 0; index < m_model->points.Size(); index++) {
        m_cpu_sorted_indices[index] = index;
        m_cpu_distances[index] = -1000.0f;
    }
#endif

    HYP_SYNC_RENDER();

    SetReady(true);
}

void GaussianSplattingInstance::Record(Frame *frame)
{
    AssertThrow(IsReady());

    const uint32 num_points = static_cast<uint32>(m_model->points.Size());

    AssertThrow(m_splat_buffer->Size() == sizeof(GaussianSplattingInstanceShaderData) * num_points);

    { // Update splat distances from camera before we sort
        struct alignas(128) {
            uint32 num_points;
        } update_splats_distances_push_constants;

        update_splats_distances_push_constants.num_points = num_points;

        m_update_splat_distances->SetPushConstants(
            &update_splats_distances_push_constants,
            sizeof(update_splats_distances_push_constants)
        );

        m_update_splat_distances->Bind(frame->GetCommandBuffer());

        m_update_splat_distances->GetDescriptorTable()->Bind(
            frame,
            m_update_splat_distances,
            {
                {
                    NAME("Scene"),
                    {
                        { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                        { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                        { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                        { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0) },
                        { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                    }
                }
            }
        );

        m_update_splat_distances->Dispatch(
            frame->GetCommandBuffer(),
            Extent3D {
                uint32((num_points + 255) / 256), 1, 1
            }
        );

        m_splat_indices_buffer->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::UNORDERED_ACCESS
        );
    }

#ifdef HYP_GAUSSIAN_SPLATTING_CPU_SORT
    { // Temporary CPU sorting -- inefficient but useful for testing
        for (SizeType index = 0; index < m_model->points.Size(); index++) {
            m_cpu_distances[index] = (g_engine->GetRenderState().GetCamera().camera.view * m_model->points[index].position).z;
        }

        std::sort(m_cpu_sorted_indices.Begin(), m_cpu_sorted_indices.End(), [&distances = m_cpu_distances](uint32 a, uint32 b) {
            return distances[a] < distances[b];
        });

        AssertThrowMsg(m_splat_indices_buffer->Size() >= m_cpu_sorted_indices.Size() * sizeof(m_cpu_sorted_indices[0]),
            "Expected buffer size to be at least %llu -- got %llu.",
            m_cpu_sorted_indices.Size() * sizeof(m_cpu_sorted_indices[0]),
            m_splat_indices_buffer->Size()
        );

        // Copy the cpu sorted indices over
        m_splat_indices_buffer->Copy(g_engine->GetGPUDevice(), MathUtil::Min(m_splat_indices_buffer->Size(), m_cpu_sorted_indices.Size() * sizeof(m_cpu_sorted_indices[0])), m_cpu_sorted_indices.Data());
    }
#else
    { // Sort splats
        constexpr uint32 block_size = 512;
        constexpr uint32 transpose_block_size = 16;

        struct alignas(128) {
            uint32 num_points;
            uint32 stage;
            uint32 h;
        } sort_splats_push_constants;

        Memory::MemSet(&sort_splats_push_constants, 0x0, sizeof(sort_splats_push_constants));

        const uint32 num_sortable_elements = uint32(MathUtil::NextPowerOf2(num_points)); // Values are stored in components of uvec4

        const uint32 width = block_size;
        const uint32 height = num_sortable_elements / block_size;

        sort_splats_push_constants.num_points = num_points;

        m_splat_indices_buffer->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::UNORDERED_ACCESS
        );


        static constexpr uint32 max_workgroup_size = 512;
        uint32 workgroup_size_x = 1;

        if (num_sortable_elements < max_workgroup_size * 2) {
            workgroup_size_x = num_sortable_elements / 2;
        } else {
            workgroup_size_x = max_workgroup_size;
        }

        AssertThrowMsg(workgroup_size_x == max_workgroup_size, "Not implemented for workgroup size < max_workgroup_size");

        uint32 h = workgroup_size_x * 2;
        const uint32 workgroup_count = num_sortable_elements / (workgroup_size_x * 2);

        AssertThrow(h < num_sortable_elements);
        AssertThrow(h % 2 == 0);

        auto DoPass = [this, frame, pc = sort_splats_push_constants, workgroup_count](BitonicSortStage stage, uint32 h) mutable
        {
            pc.stage = uint32(stage);
            pc.h = h;

            m_sort_splats->SetPushConstants(&pc, sizeof(pc));

            m_sort_splats->Bind(frame->GetCommandBuffer());

            m_sort_stage_descriptor_tables[SORT_STAGE_FIRST]->Bind(
                frame,
                m_sort_splats,
                {
                    {
                        NAME("Scene"),
                        {
                            { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                            { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                            { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                            { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0) },
                            { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                        }
                    }
                }
            );

            m_sort_splats->Dispatch(
                frame->GetCommandBuffer(),
                Extent3D { workgroup_count, 1, 1 }
            );

            m_splat_indices_buffer->InsertBarrier(
                frame->GetCommandBuffer(),
                renderer::ResourceState::UNORDERED_ACCESS
            );
        };

        DoPass(STAGE_LOCAL_BMS, h);

        h <<= 1;

        for (; h <= num_sortable_elements; h <<= 1) {
            DoPass(STAGE_BIG_FLIP, h);

            for (uint32 hh = h >> 1; hh > 1; hh >>= 1) {
                if (hh <= workgroup_size_x * 2) {
                    DoPass(STAGE_LOCAL_DISPERSE, hh);

                    break;
                } else {
                    DoPass(STAGE_BIG_DISPERSE, hh);
                }
            }
        }
    }
#endif
    { // Update splats
        struct alignas(128) {
            uint32 num_points;
        } update_splats_push_constants;

        update_splats_push_constants.num_points = num_points;

        m_update_splats->SetPushConstants(
            &update_splats_push_constants,
            sizeof(update_splats_push_constants)
        );

        m_update_splats->Bind(frame->GetCommandBuffer());

        m_update_splats->GetDescriptorTable()->Bind(
            frame,
            m_update_splats,
            {
                {
                    NAME("Scene"),
                    {
                        { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                        { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                        { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                        { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0) },
                        { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                    }
                }
            }
        );

        m_update_splats->Dispatch(
            frame->GetCommandBuffer(),
            Extent3D {
                uint32((num_points + 255) / 256), 1, 1
            }
        );

        m_indirect_buffer->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::INDIRECT_ARG
        );
    }
}

void GaussianSplattingInstance::CreateBuffers()
{
    m_splat_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
    m_splat_indices_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
    m_scene_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::CONSTANT_BUFFER);
    m_indirect_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::INDIRECT_ARGS_BUFFER);

    PUSH_RENDER_COMMAND(
        CreateGaussianSplattingInstanceBuffers,
        m_splat_buffer,
        m_splat_indices_buffer,
        m_scene_buffer,
        m_indirect_buffer,
        m_model
    );
}

void GaussianSplattingInstance::CreateShader()
{
    m_shader = g_shader_manager->GetOrCreate(NAME("GaussianSplatting"));
}

void GaussianSplattingInstance::CreateRenderGroup()
{
    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(
        m_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable()
    );

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("GaussianSplattingDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("SplatIndicesBuffer"), m_splat_indices_buffer);
        descriptor_set->SetElement(NAME("SplatInstancesBuffer"), m_splat_buffer);
        descriptor_set->SetElement(NAME("GaussianSplattingSceneShaderData"), m_scene_buffer);
    }

    DeferCreate(
        descriptor_table,
        g_engine->GetGPUDevice()
    );

    m_render_group = CreateObject<RenderGroup>(
        m_shader,
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = static_mesh_vertex_attributes//VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION
            },
            MaterialAttributes {
                .bucket         = Bucket::BUCKET_TRANSLUCENT,
                .blend_function = BlendFunction::Additive(),
                .cull_faces     = FaceCullMode::NONE,
                .flags          = MaterialAttributeFlags::NONE
            }
        ),
        descriptor_table,
        RenderGroupFlags::NONE
    );

    m_render_group->AddFramebuffer(g_engine->GetGBuffer()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffer());

    AssertThrow(InitObject(m_render_group));
}

void GaussianSplattingInstance::CreateComputePipelines()
{
    ShaderProperties base_properties;

    // UpdateSplats

    ShaderRef update_splats_shader = g_shader_manager->GetOrCreate(
        NAME("GaussianSplatting_UpdateSplats"),
        base_properties
    );

    DescriptorTableRef update_splats_descriptor_table = MakeRenderObject<DescriptorTable>(
        update_splats_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable()
    );

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = update_splats_descriptor_table->GetDescriptorSet(NAME("UpdateSplatsDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("SplatIndicesBuffer"), m_splat_indices_buffer);
        descriptor_set->SetElement(NAME("SplatInstancesBuffer"), m_splat_buffer);
        descriptor_set->SetElement(NAME("IndirectDrawCommandsBuffer"), m_indirect_buffer);
        descriptor_set->SetElement(NAME("GaussianSplattingSceneShaderData"), m_scene_buffer);
    }

    DeferCreate(
        update_splats_descriptor_table,
        g_engine->GetGPUDevice()
    );

    m_update_splats = MakeRenderObject<ComputePipeline>(
        update_splats_shader,
        update_splats_descriptor_table
    );

    DeferCreate(
        m_update_splats,
        g_engine->GetGPUDevice()
    );

    // UpdateDistances

    ShaderRef update_splat_distances_shader = g_shader_manager->GetOrCreate(
        NAME("GaussianSplatting_UpdateDistances"),
        base_properties
    );

    DescriptorTableRef update_splat_distances_descriptor_table = MakeRenderObject<DescriptorTable>(
        update_splat_distances_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable()
    );

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = update_splat_distances_descriptor_table->GetDescriptorSet(NAME("UpdateDistancesDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("SplatIndicesBuffer"), m_splat_indices_buffer);
        descriptor_set->SetElement(NAME("SplatInstancesBuffer"), m_splat_buffer);
        descriptor_set->SetElement(NAME("GaussianSplattingSceneShaderData"), m_scene_buffer);
    }

    DeferCreate(
        update_splat_distances_descriptor_table,
        g_engine->GetGPUDevice()
    );

    m_update_splat_distances = MakeRenderObject<ComputePipeline>(
        update_splat_distances_shader,
        update_splat_distances_descriptor_table
    );

    DeferCreate(
        m_update_splat_distances,
        g_engine->GetGPUDevice()
    );

    // SortSplats

    ShaderRef sort_splats_shader = g_shader_manager->GetOrCreate(
        NAME("GaussianSplatting_SortSplats"),
        base_properties
    );

    m_sort_stage_descriptor_tables.Resize(SortStage::SORT_STAGE_MAX);

    for (uint sort_stage_index = 0; sort_stage_index < SortStage::SORT_STAGE_MAX; sort_stage_index++) {
        DescriptorTableRef sort_splats_descriptor_table = MakeRenderObject<DescriptorTable>(
            sort_splats_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable()
        );

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const DescriptorSetRef &descriptor_set = sort_splats_descriptor_table->GetDescriptorSet(NAME("SortSplatsDescriptorSet"), frame_index);
            AssertThrow(descriptor_set != nullptr);

            descriptor_set->SetElement(NAME("SplatIndicesBuffer"), m_splat_indices_buffer);
            descriptor_set->SetElement(NAME("SplatInstancesBuffer"), m_splat_buffer);
            descriptor_set->SetElement(NAME("GaussianSplattingSceneShaderData"), m_scene_buffer);
        }

        DeferCreate(
            sort_splats_descriptor_table,
            g_engine->GetGPUDevice()
        );

        m_sort_stage_descriptor_tables[sort_stage_index] = std::move(sort_splats_descriptor_table);
    }

    m_sort_splats = MakeRenderObject<ComputePipeline>(
        sort_splats_shader,
        m_sort_stage_descriptor_tables[0]
    );

    DeferCreate(
        m_sort_splats,
        g_engine->GetGPUDevice()
    );
}

GaussianSplatting::GaussianSplatting()
    : BasicObject()
{
}

GaussianSplatting::~GaussianSplatting()
{
    if (IsInitCalled()) {
        m_quad_mesh.Reset();
        m_gaussian_splatting_instance.Reset();

        SafeRelease(std::move(m_staging_buffer));
        SafeRelease(std::move(m_command_buffers));
    }
}

void GaussianSplatting::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
    {
        m_quad_mesh.Reset();
        m_gaussian_splatting_instance.Reset();

        SafeRelease(std::move(m_staging_buffer));
        SafeRelease(std::move(m_command_buffers));
    }));

    static const Array<Vertex> vertices = {
        Vertex {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex {{ 1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex {{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex {{-1.0f,  1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}}
    };

    static const Array<Mesh::Index> indices = {
        0, 3, 1,
        2, 3, 1
    };

    m_quad_mesh = CreateObject<Mesh>(
        vertices,
        indices,
        renderer::Topology::TRIANGLES,
        static_mesh_vertex_attributes
    );

    InitObject(m_quad_mesh);

    InitObject(m_gaussian_splatting_instance);

    CreateBuffers();
    CreateCommandBuffers();

    SetReady(true);
}

void GaussianSplatting::SetGaussianSplattingInstance(Handle<GaussianSplattingInstance> gaussian_splatting_instance)
{
    m_gaussian_splatting_instance = std::move(gaussian_splatting_instance);

    if (IsInitCalled()) {
        InitObject(m_gaussian_splatting_instance);
    }
}

void GaussianSplatting::CreateBuffers()
{
    m_staging_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::STAGING_BUFFER);

    PUSH_RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers,
        m_staging_buffer,
        m_quad_mesh
    );
}

void GaussianSplatting::CreateCommandBuffers()
{
    for (uint frame_index = 0; frame_index < static_cast<uint>(m_command_buffers.Size()); frame_index++) {
        m_command_buffers[frame_index] = MakeRenderObject<CommandBuffer>(CommandBufferType::COMMAND_BUFFER_SECONDARY);
    }

    PUSH_RENDER_COMMAND(CreateGaussianSplattingCommandBuffers,
        m_command_buffers
    );
}

void GaussianSplatting::UpdateSplats(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    AssertReady();

    if (!m_gaussian_splatting_instance) {
        return;
    }

    AssertThrow(m_gaussian_splatting_instance->GetIndirectBuffer()->Size() == sizeof(IndirectDrawCommand));

    m_staging_buffer->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::COPY_SRC
    );

    m_gaussian_splatting_instance->GetIndirectBuffer()->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::COPY_DST
    );

    // copy zeros to buffer (to reset instance count)
    m_gaussian_splatting_instance->GetIndirectBuffer()->CopyFrom(
        frame->GetCommandBuffer(),
        m_staging_buffer,
        sizeof(IndirectDrawCommand)
    );

    m_gaussian_splatting_instance->GetIndirectBuffer()->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::INDIRECT_ARG
    );

    m_gaussian_splatting_instance->Record(frame);
}

void GaussianSplatting::Render(Frame *frame)
{
    AssertReady();

    if (!m_gaussian_splatting_instance) {
        return;
    }

    const uint frame_index = frame->GetFrameIndex();

    const GraphicsPipelineRef &pipeline = m_gaussian_splatting_instance->GetRenderGroup()->GetPipeline();

    m_command_buffers[frame_index]->Record(
        g_engine->GetGPUDevice(),
        pipeline->GetRenderPass(),
        [&](CommandBuffer *secondary)
        {
            pipeline->Bind(secondary);

            m_gaussian_splatting_instance->GetRenderGroup()->GetPipeline()->GetDescriptorTable()->Bind(
                secondary,
                frame_index,
                pipeline,
                {
                    {
                        NAME("Scene"),
                        {
                            { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                            { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                            { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                            { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0) },
                            { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                        }
                    }
                }
            );

            m_quad_mesh->RenderIndirect(
                secondary,
                m_gaussian_splatting_instance->GetIndirectBuffer().Get()
            );

            HYPERION_RETURN_OK;
        }
    );

    m_command_buffers[frame_index]->SubmitSecondary(frame->GetCommandBuffer());
}

} // namespace hyperion
