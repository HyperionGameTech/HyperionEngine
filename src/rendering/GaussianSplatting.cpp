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
    GPUBufferRef sorted_splat_buffer;
    GPUBufferRef splat_indices_buffer;
    GPUBufferRef indirect_buffer;
    GPUBufferRef noise_buffer;
    RC<GaussianSplattingModelData> model;

    RENDER_COMMAND(CreateGaussianSplattingInstanceBuffers)(
        GPUBufferRef splat_buffer,
        GPUBufferRef sorted_splat_buffer,
        GPUBufferRef splat_indices_buffer,
        GPUBufferRef indirect_buffer,
        GPUBufferRef noise_buffer,
        RC<GaussianSplattingModelData> model
    ) : splat_buffer(std::move(splat_buffer)),
        sorted_splat_buffer(std::move(sorted_splat_buffer)),
        splat_indices_buffer(std::move(splat_indices_buffer)),
        indirect_buffer(std::move(indirect_buffer)),
        noise_buffer(std::move(noise_buffer)),
        model(std::move(model))
    {
    }

    virtual Result operator()()
    {
        static constexpr UInt seed = 0xff;

        SimplexNoiseGenerator noise_generator(seed);
        auto noise_map = noise_generator.CreateBitmap(128, 128, 1024.0f);

        static_assert(sizeof(GaussianSplatShaderData) == sizeof(model->points[0]));

        HYPERION_BUBBLE_ERRORS(splat_buffer->Create(
           g_engine->GetGPUDevice(),
           model->points.Size() * sizeof(GaussianSplatShaderData)
        ));

        splat_buffer->Copy(
            g_engine->GetGPUDevice(),
            sorted_splat_buffer->size,
            model->points.Data()
        );

        HYPERION_BUBBLE_ERRORS(sorted_splat_buffer->Create(
           g_engine->GetGPUDevice(),
           model->points.Size() * sizeof(GaussianSplatShaderData)
        ));

        sorted_splat_buffer->Copy(
            g_engine->GetGPUDevice(),
            sorted_splat_buffer->size,
            model->points.Data()
        );

        HYPERION_BUBBLE_ERRORS(splat_indices_buffer->Create(
           g_engine->GetGPUDevice(),
           model->points.Size() * sizeof(UInt32)
        ));

        // Set default indices

        UInt32 *indices_buffer_data = new UInt32[model->points.Size()];

        for (UInt index = 0; index < UInt(model->points.Size()); index++) {
            indices_buffer_data[index] = index;
        }

        splat_indices_buffer->Copy(
            g_engine->GetGPUDevice(),
            splat_indices_buffer->size,
            indices_buffer_data
        );

        delete[] indices_buffer_data;

        HYPERION_BUBBLE_ERRORS(indirect_buffer->Create(
            g_engine->GetGPUDevice(),
            sizeof(IndirectDrawCommand)
        ));

        HYPERION_BUBBLE_ERRORS(noise_buffer->Create(
            g_engine->GetGPUDevice(),
            noise_map.GetByteSize() * sizeof(Float)
        ));

        // copy bytes into noise buffer
        Array<Float> unpacked_floats;
        noise_map.GetUnpackedFloats(unpacked_floats);
        AssertThrow(noise_map.GetByteSize() == unpacked_floats.Size());

        noise_buffer->Copy(
            g_engine->GetGPUDevice(),
            unpacked_floats.Size() * sizeof(Float),
            unpacked_floats.Data()
        );

        // don't need it anymore
        noise_map = Bitmap<1>();

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateGaussianSplattingInstanceDescriptors) : RenderCommand
{
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    RENDER_COMMAND(CreateGaussianSplattingInstanceDescriptors)(
        FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets
    ) : descriptor_sets(std::move(descriptor_sets))
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_BUBBLE_ERRORS(descriptor_sets[frame_index]->Create(
                g_engine->GetGPUDevice(),
                &g_engine->GetGPUInstance()->GetDescriptorPool()
            ));
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
        m_render_group.Reset();
        m_update_gaussian_splatting.Reset();

        SafeRelease(std::move(m_splat_buffer));
        SafeRelease(std::move(m_indirect_buffer));
        SafeRelease(std::move(m_noise_buffer));

        SafeRelease(std::move(m_descriptor_sets));

        m_shader.Reset();
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

void GaussianSplattingInstance::Record(CommandBuffer *command_buffer)
{

}

void GaussianSplattingInstance::CreateBuffers()
{
    m_splat_buffer = RenderObjects::Make<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
    m_sorted_splat_buffer = RenderObjects::Make<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
    m_splat_indices_buffer = RenderObjects::Make<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
    m_indirect_buffer = RenderObjects::Make<GPUBuffer>(GPUBufferType::INDIRECT_ARGS_BUFFER);
    m_noise_buffer = RenderObjects::Make<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);

    PUSH_RENDER_COMMAND(
        CreateGaussianSplattingInstanceBuffers,
        m_splat_buffer,
        m_sorted_splat_buffer,
        m_splat_indices_buffer,
        m_indirect_buffer,
        m_noise_buffer,
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
        m_descriptor_sets[frame_index] = RenderObjects::Make<DescriptorSet>();

        // splat data
        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::StorageBufferDescriptor>(0)
            ->SetElementBuffer(0, m_splat_buffer);

        // indirect rendering data
        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::StorageBufferDescriptor>(1)
            ->SetElementBuffer(0, m_indirect_buffer);

        // noise buffer
        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::StorageBufferDescriptor>(2)
            ->SetElementBuffer(0, m_noise_buffer);

        // scene
        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(4)
            ->SetElementBuffer<SceneShaderData>(0, g_engine->GetRenderData()->scenes.GetBuffers()[frame_index].get());
    
        // camera
        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(5)
            ->SetElementBuffer<CameraShaderData>(0, g_engine->GetRenderData()->cameras.GetBuffers()[frame_index].get());


        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::SamplerDescriptor>(7)
            ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerLinear());

        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::SamplerDescriptor>(8)
            ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerNearest());

        // gbuffer normals texture
        m_descriptor_sets[frame_index]
            ->AddDescriptor<ImageDescriptor>(9)
            ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_NORMALS)->GetImageView());

        // gbuffer depth texture
        m_descriptor_sets[frame_index]
            ->AddDescriptor<ImageDescriptor>(10)
            ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());
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
                .cull_faces = FaceCullMode::BACK,
                .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
            }
        )
    );

    m_render_group->AddFramebuffer(Handle<Framebuffer>(g_engine->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffer()));

    // do not use global descriptor sets for this renderer -- we will just use our own local ones
    m_render_group->GetPipeline()->SetUsedDescriptorSets(Array<const DescriptorSet *> {
        m_descriptor_sets[0].Get()
    });

    AssertThrow(InitObject(m_render_group));
}

void GaussianSplattingInstance::CreateComputePipelines()
{
    ShaderProperties properties;

    m_update_gaussian_splatting = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(
            HYP_NAME(UpdateGaussianSplatting),
            properties
        ),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    InitObject(m_update_gaussian_splatting);
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

    m_quad_mesh = MeshBuilder::Quad();
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

    AssertThrow(m_gaussian_splatting_instance->IsReady());

    m_staging_buffer->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::COPY_SRC
    );
    
    const SizeType num_points = m_gaussian_splatting_instance->GetModel()->points.Size();

    AssertThrow(m_gaussian_splatting_instance->GetIndirectBuffer()->size == sizeof(IndirectDrawCommand));
    AssertThrow(m_gaussian_splatting_instance->GetSplatBuffer()->size == sizeof(GaussianSplatShaderData) * num_points);

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

    m_gaussian_splatting_instance->GetComputePipeline()->GetPipeline()->Bind(frame->GetCommandBuffer());
    
    struct alignas(128) {
        UInt32 num_points;
    } update_gaussian_splatting_push_constants;

    update_gaussian_splatting_push_constants.num_points = static_cast<UInt32>(num_points);

    m_gaussian_splatting_instance->GetComputePipeline()->GetPipeline()->SetPushConstants(
        &update_gaussian_splatting_push_constants,
        sizeof(update_gaussian_splatting_push_constants)
    );

    m_gaussian_splatting_instance->GetComputePipeline()->GetPipeline()->SubmitPushConstants(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_gaussian_splatting_instance->GetComputePipeline()->GetPipeline(),
        m_gaussian_splatting_instance->GetDescriptorSets()[frame->GetFrameIndex()],
        0,
        FixedArray {
            HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
        }
    );

    m_gaussian_splatting_instance->GetComputePipeline()->GetPipeline()->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            UInt32((num_points + 255) / 256), 1, 1
        }
    );

    m_gaussian_splatting_instance->GetIndirectBuffer()->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::INDIRECT_ARG
    );
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
                m_gaussian_splatting_instance->GetDescriptorSets()[frame_index],
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
