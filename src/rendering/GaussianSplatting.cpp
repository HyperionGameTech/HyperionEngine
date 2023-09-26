#include "GaussianSplatting.hpp"

#include <Engine.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <util/MeshBuilder.hpp>
#include <math/MathUtil.hpp>
#include <math/Color.hpp>
#include <util/NoiseFactory.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::IndirectDrawCommand;
using renderer::Pipeline;
using renderer::StagingBuffer;
using renderer::Result;
using renderer::GPUBuffer;
using renderer::GPUBufferType;

struct RENDER_COMMAND(CreateGaussianSplattingInstanceBuffers) : RenderCommand
{
    GPUBufferRef splat_buffer;
    GPUBufferRef splat_indices_buffer;
    GPUBufferRef splat_indices_buffer_secondary;
    GPUBufferRef splat_distances_buffer;
    GPUBufferRef indirect_buffer;
    RC<GaussianSplattingModelData> model;

    RENDER_COMMAND(CreateGaussianSplattingInstanceBuffers)(
        GPUBufferRef splat_buffer,
        GPUBufferRef splat_indices_buffer,
        GPUBufferRef splat_indices_buffer_secondary,
        GPUBufferRef splat_distances_buffer,
        GPUBufferRef indirect_buffer,
        RC<GaussianSplattingModelData> model
    ) : splat_buffer(std::move(splat_buffer)),
        splat_indices_buffer(std::move(splat_indices_buffer)),
        splat_indices_buffer_secondary(std::move(splat_indices_buffer_secondary)),
        splat_distances_buffer(std::move(splat_distances_buffer)),
        indirect_buffer(std::move(indirect_buffer)),
        model(std::move(model))
    {
    }

    virtual Result operator()()
    {
        static_assert(sizeof(GaussianSplatShaderData) == sizeof(model->points[0]));

        const SizeType num_points = model->points.Size();

        HYPERION_BUBBLE_ERRORS(splat_buffer->Create(
           g_engine->GetGPUDevice(),
           num_points * sizeof(GaussianSplatShaderData)
        ));

        splat_buffer->Copy(
            g_engine->GetGPUDevice(),
            splat_buffer->size,
            model->points.Data()
        );

        const SizeType indices_buffer_size = ByteUtil::AlignAs(num_points * sizeof(UInt32), sizeof(ShaderVec4<UInt32>));
        const SizeType distances_buffer_size = ByteUtil::AlignAs(num_points * sizeof(Float32), sizeof(ShaderVec4<Float32>));

        HYPERION_BUBBLE_ERRORS(splat_indices_buffer->Create(
           g_engine->GetGPUDevice(),
           indices_buffer_size
        ));

        // Set default indices
        UInt32 *indices_buffer_data = new UInt32[indices_buffer_size / sizeof(UInt32)];

        for (SizeType index = 0; index < num_points; index++) {
            if (index >= UINT32_MAX) {
                break;
            }

            indices_buffer_data[index] = static_cast<UInt32>(index);
        }

        for (SizeType index = num_points; index < indices_buffer_size / sizeof(UInt32); index++) {
            indices_buffer_data[index] = 0;
        }

        splat_indices_buffer->Copy(
            g_engine->GetGPUDevice(),
            splat_indices_buffer->size,
            indices_buffer_data
        );

        // Create a secondary buffer, used for sorting

        HYPERION_BUBBLE_ERRORS(splat_indices_buffer_secondary->Create(
           g_engine->GetGPUDevice(),
           indices_buffer_size
        ));

        splat_indices_buffer_secondary->Copy(
            g_engine->GetGPUDevice(),
            splat_indices_buffer_secondary->size,
            indices_buffer_data
        );

        // Discard the data we used for initially setting up the buffers
        delete[] indices_buffer_data;

        HYPERION_BUBBLE_ERRORS(splat_distances_buffer->Create(
           g_engine->GetGPUDevice(),
           distances_buffer_size
        ));

        splat_distances_buffer->Memset(
            g_engine->GetGPUDevice(),
            splat_distances_buffer->size,
            0x0
        );

        HYPERION_BUBBLE_ERRORS(indirect_buffer->Create(
            g_engine->GetGPUDevice(),
            sizeof(IndirectDrawCommand)
        ));

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateGaussianSplattingInstanceDescriptors) : RenderCommand
{
    FixedArray<FixedArray<DescriptorSetRef, GaussianSplattingInstance::SortStage::SORT_STAGE_MAX>, max_frames_in_flight> descriptor_sets;

    RENDER_COMMAND(CreateGaussianSplattingInstanceDescriptors)(
        FixedArray<FixedArray<DescriptorSetRef, GaussianSplattingInstance::SortStage::SORT_STAGE_MAX>, max_frames_in_flight> descriptor_sets
    ) : descriptor_sets(std::move(descriptor_sets))
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (auto &descriptor_set : descriptor_sets[frame_index]) {
                HYPERION_BUBBLE_ERRORS(descriptor_set->Create(
                    g_engine->GetGPUDevice(),
                    &g_engine->GetGPUInstance()->GetDescriptorPool()
                ));
            }
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers) : RenderCommand
{
    GPUBufferRef staging_buffer;
    Handle<Mesh> quad_mesh;

    RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers)(
        GPUBufferRef staging_buffer,
        Handle<Mesh> quad_mesh
    ) : staging_buffer(std::move(staging_buffer)),
        quad_mesh(std::move(quad_mesh))
    {
    }

    virtual Result operator()()
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

struct RENDER_COMMAND(CreateGaussianSplattingCommandBuffers) : RenderCommand
{
    FixedArray<CommandBufferRef, max_frames_in_flight> command_buffers;

    RENDER_COMMAND(CreateGaussianSplattingCommandBuffers)(
        FixedArray<CommandBufferRef, max_frames_in_flight> command_buffers
    ) : command_buffers(command_buffers)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_BUBBLE_ERRORS(command_buffers[frame_index]->Create(
                g_engine->GetGPUInstance()->GetDevice(),
                g_engine->GetGPUInstance()->GetGraphicsCommandPool(0)
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
        SafeRelease(std::move(m_splat_distances_buffer));
        SafeRelease(std::move(m_indirect_buffer));

        for (auto &sort_stage_descriptor_sets : m_descriptor_sets) {
            for (auto &descriptor_set : sort_stage_descriptor_sets) {
                SafeRelease(std::move(descriptor_set));
            }
        }
    }
}

void GaussianSplattingInstance::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    CreateBuffers();
    CreateShader();
    CreateDescriptorSets();
    CreateRenderGroup();
    CreateComputePipelines();

    HYP_SYNC_RENDER();

    SetReady(true);
}

void GaussianSplattingInstance::Record(Frame *frame)
{
    AssertThrow(IsReady());

    const UInt32 num_points = static_cast<UInt32>(m_model->points.Size());

    AssertThrow(m_splat_buffer->size == sizeof(GaussianSplatShaderData) * num_points);

    { // Update splat distances from camera before we sort
        struct alignas(128) {
            UInt32 num_points;
        } update_splats_distances_push_constants;

        update_splats_distances_push_constants.num_points = num_points;
        
        m_update_splat_distances->GetPipeline()->Bind(frame->GetCommandBuffer());

        m_update_splat_distances->GetPipeline()->SetPushConstants(
            &update_splats_distances_push_constants,
            sizeof(update_splats_distances_push_constants)
        );

        m_update_splat_distances->GetPipeline()->SubmitPushConstants(frame->GetCommandBuffer());

        frame->GetCommandBuffer()->BindDescriptorSet(
            g_engine->GetGPUInstance()->GetDescriptorPool(),
            m_update_splat_distances->GetPipeline(),
            m_descriptor_sets[frame->GetFrameIndex()][SORT_STAGE_FIRST],
            0,
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
                HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
            }
        );

        m_update_splat_distances->GetPipeline()->Dispatch(
            frame->GetCommandBuffer(),
            Extent3D {
                UInt32((num_points + 255) / 256), 1, 1
            }
        );

        m_splat_distances_buffer->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::UNORDERED_ACCESS
        );
    }
    
    { // Sort splats
        constexpr UInt32 block_size = 512;
        constexpr UInt32 transpose_block_size = 16;

        struct alignas(128) {
            UInt32 num_points;
            UInt32 level;
            UInt32 level_mask;
            UInt32 width;
            UInt32 height;
        } sort_splats_push_constants;

        Memory::MemSet(&sort_splats_push_constants, 0x0, sizeof(sort_splats_push_constants));

        const UInt32 num_sortable_elements = MathUtil::NextPowerOf2(num_points); // Values are stored in components of uvec4

        const UInt32 width = block_size;
        const UInt32 height = num_sortable_elements / block_size;

        sort_splats_push_constants.num_points = num_points;
        
        for (UInt32 level = 2; level <= block_size; level <<= 1) {
            sort_splats_push_constants.level = level;
            sort_splats_push_constants.level_mask = level;
            sort_splats_push_constants.width = height;
            sort_splats_push_constants.height = width;

            m_sort_splats->GetPipeline()->Bind(frame->GetCommandBuffer());

            frame->GetCommandBuffer()->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                m_sort_splats->GetPipeline(),
                m_descriptor_sets[frame->GetFrameIndex()][SORT_STAGE_FIRST],
                0,
                FixedArray {
                    HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
                    HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
                }
            );
            
            m_sort_splats->GetPipeline()->SetPushConstants(
                &sort_splats_push_constants,
                sizeof(sort_splats_push_constants)
            );

            m_sort_splats->GetPipeline()->SubmitPushConstants(frame->GetCommandBuffer());

            m_sort_splats->GetPipeline()->Dispatch(
                frame->GetCommandBuffer(),
                Extent3D { num_sortable_elements / block_size, 1, 1 }
            );

            m_splat_indices_buffer->InsertBarrier(
                frame->GetCommandBuffer(),
                renderer::ResourceState::UNORDERED_ACCESS
            );
        }

        for (UInt32 level = (block_size << 1); level <= num_sortable_elements; level <<= 1) {
            sort_splats_push_constants.num_points = num_points;
            sort_splats_push_constants.level = level / block_size;
            sort_splats_push_constants.level_mask = (level & ~num_sortable_elements) / block_size;
            sort_splats_push_constants.width = width;
            sort_splats_push_constants.height = height;

            { // Transpose column data
                m_sort_splats_transpose->GetPipeline()->Bind(frame->GetCommandBuffer());

                frame->GetCommandBuffer()->BindDescriptorSet(
                    g_engine->GetGPUInstance()->GetDescriptorPool(),
                    m_sort_splats_transpose->GetPipeline(),
                    m_descriptor_sets[frame->GetFrameIndex()][SORT_STAGE_FIRST],
                    0,
                    FixedArray {
                        HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
                        HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
                    }
                );

                m_sort_splats_transpose->GetPipeline()->SetPushConstants(
                    &sort_splats_push_constants,
                    sizeof(sort_splats_push_constants)
                );

                m_sort_splats_transpose->GetPipeline()->SubmitPushConstants(frame->GetCommandBuffer());

                m_sort_splats_transpose->GetPipeline()->Dispatch(
                    frame->GetCommandBuffer(),
                    Extent3D { width / transpose_block_size, height / transpose_block_size, 1 }
                );

                m_splat_indices_buffer->InsertBarrier(
                    frame->GetCommandBuffer(),
                    renderer::ResourceState::UNORDERED_ACCESS
                );
            }

            { // Sort the transposed column data
                m_sort_splats->GetPipeline()->Bind(frame->GetCommandBuffer());

                frame->GetCommandBuffer()->BindDescriptorSet(
                    g_engine->GetGPUInstance()->GetDescriptorPool(),
                    m_sort_splats->GetPipeline(),
                    m_descriptor_sets[frame->GetFrameIndex()][SORT_STAGE_SECOND],
                    0,
                    FixedArray {
                        HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
                        HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
                    }
                );
                
                m_sort_splats->GetPipeline()->SetPushConstants(
                    &sort_splats_push_constants,
                    sizeof(sort_splats_push_constants)
                );

                m_sort_splats->GetPipeline()->SubmitPushConstants(frame->GetCommandBuffer());

                m_sort_splats->GetPipeline()->Dispatch(
                    frame->GetCommandBuffer(),
                    Extent3D { num_sortable_elements / block_size, 1, 1 }
                );

                m_splat_indices_buffer->InsertBarrier(
                    frame->GetCommandBuffer(),
                    renderer::ResourceState::UNORDERED_ACCESS
                );
            }

            { // Transpose again
                sort_splats_push_constants.level = block_size;
                sort_splats_push_constants.level_mask = level;
                sort_splats_push_constants.width = height;
                sort_splats_push_constants.height = width;
                
                m_sort_splats_transpose->GetPipeline()->Bind(frame->GetCommandBuffer());

                frame->GetCommandBuffer()->BindDescriptorSet(
                    g_engine->GetGPUInstance()->GetDescriptorPool(),
                    m_sort_splats_transpose->GetPipeline(),
                    m_descriptor_sets[frame->GetFrameIndex()][SORT_STAGE_SECOND],
                    0,
                    FixedArray {
                        HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
                        HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
                    }
                );

                m_sort_splats_transpose->GetPipeline()->SetPushConstants(
                    &sort_splats_push_constants,
                    sizeof(sort_splats_push_constants)
                );

                m_sort_splats_transpose->GetPipeline()->SubmitPushConstants(frame->GetCommandBuffer());

                m_sort_splats_transpose->GetPipeline()->Dispatch(
                    frame->GetCommandBuffer(),
                    Extent3D { height / transpose_block_size, width / transpose_block_size, 1 }
                );

                m_splat_indices_buffer->InsertBarrier(
                    frame->GetCommandBuffer(),
                    renderer::ResourceState::UNORDERED_ACCESS
                );
            }

            { // Finally, sort the transposed row data
                m_sort_splats->GetPipeline()->Bind(frame->GetCommandBuffer());

                frame->GetCommandBuffer()->BindDescriptorSet(
                    g_engine->GetGPUInstance()->GetDescriptorPool(),
                    m_sort_splats->GetPipeline(),
                    m_descriptor_sets[frame->GetFrameIndex()][SORT_STAGE_FIRST],
                    0,
                    FixedArray {
                        HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
                        HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
                    }
                );

                m_sort_splats->GetPipeline()->SetPushConstants(
                    &sort_splats_push_constants,
                    sizeof(sort_splats_push_constants)
                );

                m_sort_splats->GetPipeline()->SubmitPushConstants(frame->GetCommandBuffer());

                m_sort_splats->GetPipeline()->Dispatch(
                    frame->GetCommandBuffer(),
                    Extent3D { num_sortable_elements / block_size, 1, 1 }
                );

                m_splat_indices_buffer->InsertBarrier(
                    frame->GetCommandBuffer(),
                    renderer::ResourceState::UNORDERED_ACCESS
                );
            }
        }
    }

    { // Update splats
        struct alignas(128) {
            UInt32 num_points;
        } update_splats_push_constants;

        update_splats_push_constants.num_points = num_points;
        
        m_update_splats->GetPipeline()->Bind(frame->GetCommandBuffer());

        m_update_splats->GetPipeline()->SetPushConstants(
            &update_splats_push_constants,
            sizeof(update_splats_push_constants)
        );

        m_update_splats->GetPipeline()->SubmitPushConstants(frame->GetCommandBuffer());

        frame->GetCommandBuffer()->BindDescriptorSet(
            g_engine->GetGPUInstance()->GetDescriptorPool(),
            m_update_splats->GetPipeline(),
            m_descriptor_sets[frame->GetFrameIndex()][SORT_STAGE_FIRST],
            0,
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
                HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
            }
        );

        m_update_splats->GetPipeline()->Dispatch(
            frame->GetCommandBuffer(),
            Extent3D {
                UInt32((num_points + 255) / 256), 1, 1
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
    m_splat_buffer = RenderObjects::Make<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
    m_splat_indices_buffer = RenderObjects::Make<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
    m_splat_indices_buffer_secondary = RenderObjects::Make<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
    m_splat_distances_buffer = RenderObjects::Make<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
    m_indirect_buffer = RenderObjects::Make<GPUBuffer>(GPUBufferType::INDIRECT_ARGS_BUFFER);

    PUSH_RENDER_COMMAND(
        CreateGaussianSplattingInstanceBuffers,
        m_splat_buffer,
        m_splat_indices_buffer,
        m_splat_indices_buffer_secondary,
        m_splat_distances_buffer,
        m_indirect_buffer,
        m_model
    );
}

void GaussianSplattingInstance::CreateShader()
{
    m_shader = g_shader_manager->GetOrCreate(HYP_NAME(GaussianSplatting));

    InitObject(m_shader);
}

void GaussianSplattingInstance::CreateDescriptorSets()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        for (UInt sort_stage_index = 0; sort_stage_index < SortStage::SORT_STAGE_MAX; sort_stage_index++) {
            m_descriptor_sets[frame_index][sort_stage_index] = RenderObjects::Make<DescriptorSet>();

            // splat data
            m_descriptor_sets[frame_index][sort_stage_index]
                ->AddDescriptor<renderer::StorageBufferDescriptor>(0)
                ->SetElementBuffer(0, m_splat_buffer);

            // indirect rendering data
            m_descriptor_sets[frame_index][sort_stage_index]
                ->AddDescriptor<renderer::StorageBufferDescriptor>(1)
                ->SetElementBuffer(0, m_indirect_buffer);

            // splat data distances from camera
            m_descriptor_sets[frame_index][sort_stage_index]
                ->AddDescriptor<renderer::StorageBufferDescriptor>(4)
                ->SetElementBuffer(0, m_splat_distances_buffer);

            // scene
            m_descriptor_sets[frame_index][sort_stage_index]
                ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(5)
                ->SetElementBuffer<SceneShaderData>(0, g_engine->GetRenderData()->scenes.GetBuffers()[frame_index].get());
        
            // camera
            m_descriptor_sets[frame_index][sort_stage_index]
                ->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(6)
                ->SetElementBuffer<CameraShaderData>(0, g_engine->GetRenderData()->cameras.GetBuffers()[frame_index].get());


            m_descriptor_sets[frame_index][sort_stage_index]
                ->AddDescriptor<renderer::SamplerDescriptor>(7)
                ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerLinear());

            m_descriptor_sets[frame_index][sort_stage_index]
                ->AddDescriptor<renderer::SamplerDescriptor>(8)
                ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerNearest());

            switch (sort_stage_index) {
            case SortStage::SORT_STAGE_FIRST:
                // splat data indices
                m_descriptor_sets[frame_index][sort_stage_index]
                    ->AddDescriptor<renderer::StorageBufferDescriptor>(3)
                    ->SetElementBuffer(0, m_splat_indices_buffer);

                m_descriptor_sets[frame_index][sort_stage_index]
                    ->AddDescriptor<renderer::StorageBufferDescriptor>(9)
                    ->SetElementBuffer(0, m_splat_indices_buffer);

                m_descriptor_sets[frame_index][sort_stage_index]
                    ->AddDescriptor<renderer::StorageBufferDescriptor>(10)
                    ->SetElementBuffer(0, m_splat_indices_buffer_secondary);

                break;
            case SortStage::SORT_STAGE_SECOND:
                // splat data indices
                m_descriptor_sets[frame_index][sort_stage_index]
                    ->AddDescriptor<renderer::StorageBufferDescriptor>(3)
                    ->SetElementBuffer(0, m_splat_indices_buffer_secondary);

                m_descriptor_sets[frame_index][sort_stage_index]
                    ->AddDescriptor<renderer::StorageBufferDescriptor>(9)
                    ->SetElementBuffer(0, m_splat_indices_buffer_secondary);

                m_descriptor_sets[frame_index][sort_stage_index]
                    ->AddDescriptor<renderer::StorageBufferDescriptor>(10)
                    ->SetElementBuffer(0, m_splat_indices_buffer);

                break;
            default:
                AssertThrowMsg(false, "Invalid sort stage");
            }
        }
    }

    PUSH_RENDER_COMMAND(
        CreateGaussianSplattingInstanceDescriptors,
        m_descriptor_sets
    );
}

void GaussianSplattingInstance::CreateRenderGroup()
{
    m_render_group = CreateObject<RenderGroup>(
        Handle<Shader>(m_shader),
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = renderer::static_mesh_vertex_attributes//VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION
            },
            MaterialAttributes {
                .bucket = Bucket::BUCKET_TRANSLUCENT,
                .blend_mode = BlendMode::ADDITIVE,
                .cull_faces = FaceCullMode::NONE,
                .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
            }
        )
    );

    m_render_group->AddFramebuffer(Handle<Framebuffer>(g_engine->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffer()));

    // do not use global descriptor sets for this renderer -- we will just use our own local ones
    m_render_group->GetPipeline()->SetUsedDescriptorSets(Array<const DescriptorSet *> {
        m_descriptor_sets[0][0].Get()
    });

    AssertThrow(InitObject(m_render_group));
}

void GaussianSplattingInstance::CreateComputePipelines()
{
    ShaderProperties base_properties;

    m_update_splats = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(
            HYP_NAME(GaussianSplatting_UpdateSplats),
            base_properties
        ),
        Array<const DescriptorSet *> { m_descriptor_sets[0][0].Get() }
    );

    InitObject(m_update_splats);

    m_update_splat_distances = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(
            HYP_NAME(GaussianSplatting_UpdateDistances),
            base_properties
        ),
        Array<const DescriptorSet *> { m_descriptor_sets[0][0].Get() }
    );

    InitObject(m_update_splat_distances);

    m_sort_splats = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(
            HYP_NAME(GaussianSplatting_SortSplats),
            ShaderProperties(base_properties).Set("MODE_SORT")
        ),
        Array<const DescriptorSet *> { m_descriptor_sets[0][0].Get() }
    );

    InitObject(m_sort_splats);

    m_sort_splats_transpose = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(
            HYP_NAME(GaussianSplatting_SortSplats),
            ShaderProperties(base_properties).Set("MODE_TRANSPOSE")
        ),
        Array<const DescriptorSet *> { m_descriptor_sets[0][0].Get() }
    );

    InitObject(m_sort_splats_transpose);
}

GaussianSplatting::GaussianSplatting()
    : EngineComponentBase()
{
}

GaussianSplatting::~GaussianSplatting()
{

}

void GaussianSplatting::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    

    static const std::vector<Vertex> vertices = {
        Vertex {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex {{ 1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex {{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex {{-1.0f,  1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}}
    };
    static const std::vector<Mesh::Index> indices = {
        0, 3, 1,
        2, 3, 1
    };

    m_quad_mesh =  CreateObject<Mesh>(
        vertices,
        indices,
        renderer::Topology::TRIANGLES,
        renderer::static_mesh_vertex_attributes
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
    m_staging_buffer = RenderObjects::Make<GPUBuffer>(GPUBufferType::STAGING_BUFFER);

    RenderCommands::Push<RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers)>(
        m_staging_buffer,
        m_quad_mesh
    );
}

void GaussianSplatting::CreateCommandBuffers()
{
    for (UInt frame_index = 0; frame_index < static_cast<UInt>(m_command_buffers.Size()); frame_index++) {
        m_command_buffers[frame_index] = RenderObjects::Make<CommandBuffer>(CommandBuffer::Type::COMMAND_BUFFER_SECONDARY);
    }

    RenderCommands::Push<RENDER_COMMAND(CreateGaussianSplattingCommandBuffers)>(
        m_command_buffers
    );
}

void GaussianSplatting::UpdateSplats(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    if (!m_gaussian_splatting_instance) {
        return;
    }

    AssertThrow(m_gaussian_splatting_instance->GetIndirectBuffer()->size == sizeof(IndirectDrawCommand));

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

    const UInt frame_index = frame->GetFrameIndex();

    const GraphicsPipelineRef &pipeline = m_gaussian_splatting_instance->GetRenderGroup()->GetPipeline();

    m_command_buffers[frame_index]->Record(
        g_engine->GetGPUDevice(),
        pipeline->GetConstructionInfo().render_pass,
        [&](CommandBuffer *secondary) {
            pipeline->Bind(secondary);

            secondary->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                pipeline,
                m_gaussian_splatting_instance->GetDescriptorSets()[frame_index][GaussianSplattingInstance::SortStage::SORT_STAGE_FIRST],
                0,
                FixedArray {
                    HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
                    HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
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

} // namespace hyperion::v2
