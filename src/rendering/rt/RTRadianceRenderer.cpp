#include <rendering/rt/RTRadianceRenderer.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <Engine.hpp>

#include <core/Memory.hpp>

#include "ProbeSystem.hpp"

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::StorageImageDescriptor;
using renderer::StorageBufferDescriptor;
using renderer::TlasDescriptor;
using renderer::SamplerDescriptor;
using renderer::ResourceState;
using renderer::Result;

enum RTRadianceUpdates : uint32
{
    RT_RADIANCE_UPDATES_NONE = 0x0,
    RT_RADIANCE_UPDATES_TLAS = 0x1,
    RT_RADIANCE_UPDATES_SHADOW_MAP = 0x2
};

struct alignas(16) RTRadianceUniforms
{
    uint32 num_bound_lights;
    uint32 _pad0, _pad1, _pad2;
    uint32 light_indices[16];
};

#pragma region Render commands

struct RENDER_COMMAND(CreateRTRadianceDescriptorSets) : renderer::RenderCommand
{
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;
    FixedArray<ImageViewRef, max_frames_in_flight> image_views;

    RENDER_COMMAND(CreateRTRadianceDescriptorSets)(
        const FixedArray<DescriptorSetRef, max_frames_in_flight> &descriptor_sets,
        const FixedArray<ImageViewRef, max_frames_in_flight> &image_views
    ) : descriptor_sets(descriptor_sets),
        image_views(image_views)
    {
    }

    virtual Result operator()()
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            // create our own descriptor sets
            AssertThrow(descriptor_sets[frame_index] != nullptr);
            
            HYPERION_BUBBLE_ERRORS(descriptor_sets[frame_index]->Create(
                g_engine->GetGPUDevice(),
                &g_engine->GetGPUInstance()->GetDescriptorPool()
            ));

            // Add the final result to the global descriptor set
            DescriptorSetRef descriptor_set_globals = g_engine->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::RT_RADIANCE_RESULT)
                ->SetElementSRV(0, image_views[frame_index]);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateRTRadiancePipeline) : renderer::RenderCommand
{
    RaytracingPipelineRef pipeline;
    ShaderProgramRef shader_program;

    RENDER_COMMAND(CreateRTRadiancePipeline)(const RaytracingPipelineRef &pipeline, const ShaderProgramRef &shader_program)
        : pipeline(pipeline),
          shader_program(shader_program)
    {
    }

    virtual Result operator()()
    {
        return pipeline->Create(
            g_engine->GetGPUDevice(),
            shader_program,
            &g_engine->GetGPUInstance()->GetDescriptorPool()
        );
    }
};

struct RENDER_COMMAND(RemoveRTRadianceDescriptors) : renderer::RenderCommand
{
    virtual Result operator()()
    {
        auto result = Result::OK;

        // remove result image from global descriptor set
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            DescriptorSetRef descriptor_set_globals = g_engine->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            // set to placeholder image
            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::RT_RADIANCE_RESULT)
                ->SetElementSRV(0, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        return result;
    }
};

struct RENDER_COMMAND(CreateRTRadianceUniformBuffer) : renderer::RenderCommand
{
    GPUBufferRef uniform_buffer;

    RENDER_COMMAND(CreateRTRadianceUniformBuffer)(const GPUBufferRef &uniform_buffer)
        : uniform_buffer(uniform_buffer)
    {
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(g_engine->GetGPUDevice(), sizeof(RTRadianceUniforms)));
        uniform_buffer->Memset(g_engine->GetGPUDevice(), sizeof(RTRadianceUniforms), 0x00);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

Result RTRadianceRenderer::ImageOutput::Create(Device *device)
{
    AssertThrow(image.IsValid());
    AssertThrow(image_view.IsValid());

    HYPERION_BUBBLE_ERRORS(image->Create(device));
    HYPERION_BUBBLE_ERRORS(image_view->Create(device, image));

    HYPERION_RETURN_OK;
}

RTRadianceRenderer::RTRadianceRenderer(const Extent2D &extent, RTRadianceRendererOptions options)
    : m_extent(extent),
      m_options(options),
      m_updates { RT_RADIANCE_UPDATES_NONE, RT_RADIANCE_UPDATES_NONE }
{
}

RTRadianceRenderer::~RTRadianceRenderer()
{
}

void RTRadianceRenderer::Create()
{
    AssertThrowMsg(
        InitObject(m_tlas),
        "Failed to initialize the top level acceleration structure!"
    );

    CreateImages();
    CreateUniformBuffer();
    CreateTemporalBlending();
    CreateDescriptorSets();
    CreateRaytracingPipeline();
}

void RTRadianceRenderer::Destroy()
{
    m_temporal_blending->Destroy();

    m_shader.Reset();

    SafeRelease(std::move(m_raytracing_pipeline));

    SafeRelease(std::move(m_uniform_buffer));

    // release our owned descriptor sets
    for (auto &descriptor_set : m_descriptor_sets) {
        SafeRelease(std::move(descriptor_set));
    }
    
    // remove result image from global descriptor set
    g_safe_deleter->SafeReleaseHandle(std::move(m_texture));

    PUSH_RENDER_COMMAND(RemoveRTRadianceDescriptors);

    HYP_SYNC_RENDER();
}

void RTRadianceRenderer::UpdateUniforms(Frame *frame)
{
    RTRadianceUniforms uniforms;

    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

    const uint32 num_bound_lights = MathUtil::Min(uint32(g_engine->GetRenderState().lights.Size()), 16);

    for (uint32 index = 0; index < num_bound_lights; index++) {
        uniforms.light_indices[index] = (g_engine->GetRenderState().lights.Data() + index)->first.ToIndex();
    }

    uniforms.num_bound_lights = num_bound_lights;

    m_uniform_buffer->Copy(g_engine->GetGPUDevice(), sizeof(uniforms), &uniforms);


    if (m_updates[frame->GetFrameIndex()]) {
        m_descriptor_sets[frame->GetFrameIndex()]->ApplyUpdates(g_engine->GetGPUDevice());

        m_updates[frame->GetFrameIndex()] = RT_RADIANCE_UPDATES_NONE;
    }
}

void RTRadianceRenderer::SubmitPushConstants(CommandBuffer *command_buffer)
{
    /*struct alignas(128) {
        uint32 light_indices[16];
        uint32 num_bound_lights;
    } push_constants;

    m_raytracing_pipeline->SubmitPushConstants(command_buffer);

    DebugLog(LogType::Debug, "num_bound_lights = %u\n", num_bound_lights);*/
}

void RTRadianceRenderer::Render(Frame *frame)
{
    // Used for environment map
    ID<EnvProbe> env_probe_id;

    if (g_engine->render_state.bound_env_probes[ENV_PROBE_TYPE_REFLECTION].Any()) {
        env_probe_id = g_engine->render_state.bound_env_probes[ENV_PROBE_TYPE_REFLECTION].Front().first;
    }

    UpdateUniforms(frame);

    m_raytracing_pipeline->Bind(frame->GetCommandBuffer());

    SubmitPushConstants(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_raytracing_pipeline,
        m_descriptor_sets[frame->GetFrameIndex()],
        0
    );

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_raytracing_pipeline,
        DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE, frame->GetFrameIndex()),
        1,
        FixedArray {
            HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(Light, 0),
            HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(EnvProbe, env_probe_id.ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
        }
    );

    frame->GetCommandBuffer()->BindDescriptorSet(
        g_engine->GetGPUInstance()->GetDescriptorPool(),
        m_raytracing_pipeline,
        DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS, frame->GetFrameIndex()),
        2
    );

    m_texture->GetImage()->GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::UNORDERED_ACCESS
    );

    const Extent3D image_extent = m_texture->GetImage()->GetExtent();
    const SizeType num_pixels = image_extent.Size();
    const SizeType half_num_pixels = num_pixels / 2;

    m_raytracing_pipeline->TraceRays(
        g_engine->GetGPUDevice(),
        frame->GetCommandBuffer(),
        Extent3D {
            uint32(half_num_pixels), 1, 1
        }
    );

    m_texture->GetImage()->GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::SHADER_RESOURCE
    );

    m_temporal_blending->Render(frame);
}

void RTRadianceRenderer::CreateImages()
{
    m_texture = CreateObject<Texture>(Texture2D(
        m_extent,
        InternalFormat::RGBA8,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

    m_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_texture);
}

void RTRadianceRenderer::CreateUniformBuffer()
{
    m_uniform_buffer = MakeRenderObject<GPUBuffer>(UniformBuffer());

    PUSH_RENDER_COMMAND(CreateRTRadianceUniformBuffer, m_uniform_buffer);
}

void RTRadianceRenderer::ApplyTLASUpdates(RTUpdateStateFlags flags)
{
    if (!flags) {
        return;
    }
    
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE) {
            // update acceleration structure in descriptor set
            m_descriptor_sets[frame_index]->GetDescriptor(0)
                ->SetElementAccelerationStructure(0, m_tlas->GetInternalTLAS());
        }

        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS) {
            // update mesh descriptions buffer in descriptor set
            m_descriptor_sets[frame_index]->GetDescriptor(2)
                ->SetElementBuffer(0, m_tlas->GetInternalTLAS()->GetMeshDescriptionsBuffer());
        }

        m_updates[frame_index] |= RT_RADIANCE_UPDATES_TLAS;
    }
}

void RTRadianceRenderer::CreateDescriptorSets()
{
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = MakeRenderObject<DescriptorSet>();

        descriptor_set->GetOrAddDescriptor<TlasDescriptor>(0)
            ->SetElementAccelerationStructure(0, m_tlas->GetInternalTLAS());

        descriptor_set->GetOrAddDescriptor<StorageImageDescriptor>(1)
            ->SetElementUAV(0, m_texture->GetImageView());
        
        // mesh descriptions
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(2)
            ->SetElementBuffer(0, m_tlas->GetInternalTLAS()->GetMeshDescriptionsBuffer());
        
        // materials
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(3)
            ->SetElementBuffer(0, g_engine->GetRenderData()->materials.GetBuffer());
        
        // entities
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(4)
            ->SetElementBuffer(0, g_engine->GetRenderData()->objects.GetBuffer());
        
        // lights
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(5)
            ->SetElementBuffer(0, g_engine->GetRenderData()->lights.GetBuffer());
        
        descriptor_set // gbuffer normals texture
            ->AddDescriptor<ImageDescriptor>(6)
            ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
                .GetGBufferAttachment(GBUFFER_RESOURCE_NORMALS)->GetImageView());

        descriptor_set // gbuffer material texture
            ->AddDescriptor<ImageDescriptor>(7)
            ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
                .GetGBufferAttachment(GBUFFER_RESOURCE_MATERIAL)->GetImageView());

        descriptor_set // gbuffer depth texture
            ->AddDescriptor<ImageDescriptor>(8)
            ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
                .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());

        descriptor_set // gbuffer albedo texture
            ->AddDescriptor<ImageDescriptor>(9)
            ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
                .GetGBufferAttachment(GBUFFER_RESOURCE_ALBEDO)->GetImageView());

        // nearest sampler
        descriptor_set
            ->AddDescriptor<renderer::SamplerDescriptor>(10)
            ->SetElementSampler(0, g_engine->GetPlaceholderData()->GetSamplerNearest());

        // linear sampler
        descriptor_set
            ->AddDescriptor<renderer::SamplerDescriptor>(11)
            ->SetElementSampler(0, g_engine->GetPlaceholderData()->GetSamplerLinear());

        // blue noise buffer
        descriptor_set
            ->AddDescriptor<renderer::StorageBufferDescriptor>(12)
            ->SetElementBuffer(0, g_engine->GetDeferredRenderer().GetBlueNoiseBuffer());

        // RT radiance uniforms
        descriptor_set
            ->AddDescriptor<renderer::UniformBufferDescriptor>(13)
            ->SetElementBuffer(0, m_uniform_buffer);

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    PUSH_RENDER_COMMAND(
        CreateRTRadianceDescriptorSets,
        m_descriptor_sets,
        FixedArray<ImageViewRef, max_frames_in_flight> {
            m_temporal_blending->GetImageOutput(0).image_view,
            m_temporal_blending->GetImageOutput(1).image_view
        }
    );
}

void RTRadianceRenderer::CreateRaytracingPipeline()
{
    if (m_options & RT_RADIANCE_RENDERER_OPTION_PATHTRACER) {
        m_shader = g_shader_manager->GetOrCreate(HYP_NAME(PathTracer));
    } else {
        m_shader = g_shader_manager->GetOrCreate(HYP_NAME(RTRadiance));
    }

    if (!InitObject(m_shader)) {
        return;
    }

    m_raytracing_pipeline = MakeRenderObject<RaytracingPipeline>(
        Array<DescriptorSetRef> {
            m_descriptor_sets[0],
            g_engine->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE),
            g_engine->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS)
        }
    );

    g_engine->callbacks.Once(EngineCallback::CREATE_RAYTRACING_PIPELINES, [this](...) {
        PUSH_RENDER_COMMAND(
            CreateRTRadiancePipeline,
            m_raytracing_pipeline,
            m_shader->GetShaderProgram()
        );
    });
}

void RTRadianceRenderer::CreateTemporalBlending()
{
    m_temporal_blending.Reset(new TemporalBlending(
        m_extent,
        InternalFormat::RGBA8,
        TemporalBlendTechnique::TECHNIQUE_1,
        (m_options & RT_RADIANCE_RENDERER_OPTION_PATHTRACER)
            ? TemporalBlendFeedback::HIGH
            : TemporalBlendFeedback::LOW,
        FixedArray<ImageViewRef, max_frames_in_flight> {
            m_texture->GetImageView(),
            m_texture->GetImageView(),
        }
    ));

    m_temporal_blending->Create();
}

} // namespace hyperion::v2