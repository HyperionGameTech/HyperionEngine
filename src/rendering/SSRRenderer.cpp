#include "SSRRenderer.hpp"
#include <Engine.hpp>
#include <Threads.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;
using renderer::ImageRect;
using renderer::ShaderVec2;
using renderer::Result;

struct alignas(16) SSRParams
{
    ShaderVec2<Int32> dimensions;
    Float32 ray_step,
        num_iterations,
        max_ray_distance,
        distance_bias,
        offset,
        eye_fade_start,
        eye_fade_end,
        screen_edge_fade_start,
        screen_edge_fade_end;
};

struct RENDER_COMMAND(CreateSSRImageOutputs) : RenderCommand
{
    Extent2D extent;
    FixedArray<SSRRenderer::ImageOutput, 4> *image_outputs;
    SSRRenderer::ImageOutput *radius_outputs;

    RENDER_COMMAND(CreateSSRImageOutputs)(
        Extent2D extent,
        FixedArray<SSRRenderer::ImageOutput, 4> *image_outputs,
        SSRRenderer::ImageOutput *radius_outputs
    ) : extent(extent),
        image_outputs(image_outputs),
        radius_outputs(radius_outputs)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (auto &image_output : image_outputs[frame_index]) {
                image_output = SSRRenderer::ImageOutput {
                    .image = StorageImage(
                        Extent3D(extent),
                        InternalFormat::RGBA16F,
                        ImageType::TEXTURE_TYPE_2D,
                        nullptr
                    )
                };
            }

            radius_outputs[frame_index] = SSRRenderer::ImageOutput {
                .image = StorageImage(
                    Extent3D(extent),
                    InternalFormat::R8,
                    ImageType::TEXTURE_TYPE_2D,
                    nullptr
                )
            };
        }

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (auto &image_output : image_outputs[frame_index]) {
                image_output.Create(Engine::Get()->GetGPUDevice());
            }

            radius_outputs[frame_index].Create(Engine::Get()->GetGPUDevice());
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateSSRUniformBuffer) : RenderCommand
{
    Extent2D extent;
    UniquePtr<UniformBuffer> *uniform_buffers;

    RENDER_COMMAND(CreateSSRUniformBuffer)(
        Extent2D extent,
        UniquePtr<UniformBuffer> *uniform_buffers
    ) : extent(extent),
        uniform_buffers(uniform_buffers)
    {
    }

    virtual Result operator()()
    {
        SSRParams ssr_params {
            .dimensions = Vector2(extent),
            .ray_step = 4.0f,
            .num_iterations = 128.0f,
            .max_ray_distance = 100.0f,
            .distance_bias = 0.15f,
            .offset = 0.01f,
            .eye_fade_start = 0.75f,
            .eye_fade_end = 0.98f,
            .screen_edge_fade_start = 0.80f,
            .screen_edge_fade_end = 0.995f
        };

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            AssertThrow(uniform_buffers[frame_index] != nullptr);
            
            HYPERION_BUBBLE_ERRORS(uniform_buffers[frame_index]->Create(
                Engine::Get()->GetGPUDevice(),
                sizeof(ssr_params)
            ));

            uniform_buffers[frame_index]->Copy(
                Engine::Get()->GetGPUDevice(),
                sizeof(ssr_params),
                &ssr_params
            );
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateSSRDescriptors) : RenderCommand
{
    UniquePtr<renderer::DescriptorSet> *descriptor_sets;
    FixedArray<renderer::ImageView *, max_frames_in_flight> image_views;

    RENDER_COMMAND(CreateSSRDescriptors)(
        UniquePtr<renderer::DescriptorSet> *descriptor_sets,
        FixedArray<renderer::ImageView *, max_frames_in_flight> &&image_views
    ) : descriptor_sets(descriptor_sets),
        image_views(std::move(image_views))
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            // create our own descriptor sets
            AssertThrow(descriptor_sets[frame_index] != nullptr);
            
            HYPERION_BUBBLE_ERRORS(descriptor_sets[frame_index]->Create(
                Engine::Get()->GetGPUDevice(),
                &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
            ));

            // Add the final result to the global descriptor set
            auto *descriptor_set_globals = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::SSR_RESULT)
                ->SetElementSRV(0, image_views[frame_index]);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroySSRInstance) : RenderCommand
{
    FixedArray<SSRRenderer::ImageOutput, 4> *image_outputs;
    SSRRenderer::ImageOutput *radius_outputs;

    RENDER_COMMAND(DestroySSRInstance)(
        FixedArray<SSRRenderer::ImageOutput, 4> *image_outputs,
        SSRRenderer::ImageOutput *radius_outputs
    ) : image_outputs(image_outputs),
        radius_outputs(radius_outputs)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (UInt j = 0; j < UInt(image_outputs[frame_index].Size()); j++) {
                image_outputs[frame_index][j].Destroy(Engine::Get()->GetGPUDevice());
            }

            radius_outputs[frame_index].Destroy(Engine::Get()->GetGPUDevice());

            // unset final result from the global descriptor set
            auto *descriptor_set_globals = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::SSR_RESULT)
                ->SetElementSRV(0, &Engine::Get()->GetPlaceholderData().GetImageView2D1x1R8());
        }

        HYPERION_RETURN_OK;
    }
};

SSRRenderer::SSRRenderer(const Extent2D &extent)
    : m_extent(extent),
      m_temporal_blending(
          extent,
          InternalFormat::RGBA16F,
          TemporalBlendTechnique::TECHNIQUE_3,
          FixedArray<ImageView *, max_frames_in_flight> { &m_image_outputs[0].Back().image_view, &m_image_outputs[1].Back().image_view }
      ),
      m_is_rendered(false)
{
}

SSRRenderer::~SSRRenderer()
{
}

void SSRRenderer::Create()
{
    RenderCommands::Push<RENDER_COMMAND(CreateSSRImageOutputs)>(
        m_extent,
        m_image_outputs.Data(),
        m_radius_output.Data()
    );

    if (use_temporal_blending) {
        m_temporal_blending.Create();
    }

    CreateUniformBuffers();
    CreateDescriptorSets();
    CreateComputePipelines();
}

void SSRRenderer::Destroy()
{
    m_is_rendered = false;

    m_write_uvs.Reset();
    m_sample.Reset();
    m_blur_hor.Reset();
    m_blur_vert.Reset();

    if (use_temporal_blending) {
        m_temporal_blending.Destroy();
    }

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        Engine::Get()->SafeRelease(std::move(m_descriptor_sets[frame_index]));
        Engine::Get()->SafeRelease(std::move(m_uniform_buffers[frame_index]));
    }

    RenderCommands::Push<RENDER_COMMAND(DestroySSRInstance)>(
        m_image_outputs.Data(),
        m_radius_output.Data()
    );

    HYP_SYNC_RENDER();
}

void SSRRenderer::CreateUniformBuffers()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_uniform_buffers[frame_index] = UniquePtr<UniformBuffer>::Construct();
    }

    RenderCommands::Push<RENDER_COMMAND(CreateSSRUniformBuffer)>(
        m_extent,
        m_uniform_buffers.Data()
    );
}

void SSRRenderer::CreateDescriptorSets()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = UniquePtr<DescriptorSet>::Construct();

        descriptor_set // 1st stage -- trace, write UVs
            ->AddDescriptor<renderer::StorageImageDescriptor>(0)
            ->SetElementUAV(0, &m_image_outputs[frame_index][0].image_view);

        descriptor_set // 2nd stage -- sample
            ->AddDescriptor<renderer::StorageImageDescriptor>(1)
            ->SetElementUAV(0, &m_image_outputs[frame_index][1].image_view);

        descriptor_set // 2nd stage -- write radii
            ->AddDescriptor<renderer::StorageImageDescriptor>(2)
            ->SetElementUAV(0, &m_radius_output[frame_index].image_view);

        descriptor_set // 3rd stage -- blur horizontal
            ->AddDescriptor<renderer::StorageImageDescriptor>(3)
            ->SetElementUAV(0, &m_image_outputs[frame_index][2].image_view);

        descriptor_set // 3rd stage -- blur vertical
            ->AddDescriptor<renderer::StorageImageDescriptor>(4)
            ->SetElementUAV(0, &m_image_outputs[frame_index][3].image_view);

        descriptor_set // 1st stage -- trace, write UVs
            ->AddDescriptor<renderer::ImageDescriptor>(5)
            ->SetElementSRV(0, &m_image_outputs[frame_index][0].image_view);

        descriptor_set // 2nd stage -- sample
            ->AddDescriptor<renderer::ImageDescriptor>(6)
            ->SetElementSRV(0, &m_image_outputs[frame_index][1].image_view);

        descriptor_set // 2nd stage -- write radii
            ->AddDescriptor<renderer::ImageDescriptor>(7)
            ->SetElementSRV(0, &m_radius_output[frame_index].image_view);

        descriptor_set // 3rd stage -- blur horizontal
            ->AddDescriptor<renderer::ImageDescriptor>(8)
            ->SetElementSRV(0, &m_image_outputs[frame_index][2].image_view);

        descriptor_set // 3rd stage -- blur vertical
            ->AddDescriptor<renderer::ImageDescriptor>(9)
            ->SetElementSRV(0, &m_image_outputs[frame_index][3].image_view);

        descriptor_set // gbuffer albedo texture
            ->AddDescriptor<renderer::ImageDescriptor>(10)
            ->SetElementSRV(0, &Engine::Get()->GetDeferredRenderer().GetMipChain(frame_index)->GetImageView());

        descriptor_set // gbuffer normals texture
            ->AddDescriptor<renderer::ImageDescriptor>(11)
            ->SetElementSRV(0, Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
                .GetGBufferAttachment(GBUFFER_RESOURCE_NORMALS)->GetImageView());

        descriptor_set // gbuffer material texture
            ->AddDescriptor<renderer::ImageDescriptor>(12)
            ->SetElementSRV(0, Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
                .GetGBufferAttachment(GBUFFER_RESOURCE_MATERIAL)->GetImageView());

        descriptor_set // gbuffer albedo texture
            ->AddDescriptor<renderer::ImageDescriptor>(13)
            ->SetElementSRV(0, Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
                .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());

        // nearest sampler
        descriptor_set
            ->AddDescriptor<renderer::SamplerDescriptor>(14)
            ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerNearest());

        // linear sampler
        descriptor_set
            ->AddDescriptor<renderer::SamplerDescriptor>(15)
            ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerLinear());

        // scene data
        descriptor_set
            ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(16)
            ->SetElementBuffer<SceneShaderData>(0, Engine::Get()->GetRenderData()->scenes.GetBuffers()[frame_index].get());

        // uniforms
        descriptor_set
            ->AddDescriptor<renderer::UniformBufferDescriptor>(17)
            ->SetElementBuffer(0, m_uniform_buffers[frame_index].Get());

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    RenderCommands::Push<RENDER_COMMAND(CreateSSRDescriptors)>(
        m_descriptor_sets.Data(),
        FixedArray<renderer::ImageView *, max_frames_in_flight> {
            use_temporal_blending ? &m_temporal_blending.GetImageOutput(0).image_view : &m_image_outputs[0].Back().image_view,
            use_temporal_blending ? &m_temporal_blending.GetImageOutput(1).image_view : &m_image_outputs[1].Back().image_view
        }
    );
}

void SSRRenderer::CreateComputePipelines()
{
    m_write_uvs = CreateObject<ComputePipeline>(
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("SSRWriteUVs")),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    InitObject(m_write_uvs);

    m_sample = CreateObject<ComputePipeline>(
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("SSRSample")),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    InitObject(m_sample);

    m_blur_hor = CreateObject<ComputePipeline>(
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("SSRBlurHor")),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    InitObject(m_blur_hor);

    m_blur_vert = CreateObject<ComputePipeline>(
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("SSRBlurVert")),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    InitObject(m_blur_vert);
}

void SSRRenderer::Render(Frame *frame)
{
    const auto &scene_binding = Engine::Get()->render_state.GetScene();
    const auto scene_index = scene_binding ? scene_binding.id.value - 1 : 0;

    auto *command_buffer = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    /* ========== BEGIN SSR ========== */
    DebugMarker begin_ssr_marker(command_buffer, "Begin SSR");
    
    // PASS 1 -- write UVs

    m_image_outputs[frame_index][0].image.GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

    m_write_uvs->GetPipeline()->Bind(command_buffer);

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_write_uvs->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0,
        FixedArray { UInt32(Engine::Get()->GetRenderState().GetScene().id.ToIndex() * sizeof(SceneShaderData))}
    );

    m_write_uvs->GetPipeline()->Dispatch(command_buffer, Extent3D(m_extent) / Extent3D { 8, 8, 1 });

    // transition the UV image back into read state
    m_image_outputs[frame_index][0].image.GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    // PASS 2 - sample textures

    // put sample image in writeable state
    m_image_outputs[frame_index][1].image.GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);
    // put radius image in writeable state
    m_radius_output[frame_index].image.GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

    m_sample->GetPipeline()->Bind(command_buffer);

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_sample->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0,
        FixedArray { static_cast<UInt32>(Engine::Get()->GetRenderState().GetScene().id.ToIndex() * sizeof(SceneShaderData))}
    );

    m_sample->GetPipeline()->Dispatch(command_buffer, Extent3D(m_extent) / Extent3D { 8, 8, 1 });

    // transition sample image back into read state
    m_image_outputs[frame_index][1].image.GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    // transition radius image back into read state
    m_radius_output[frame_index].image.GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    // PASS 3 - blur image using radii in output from previous stage

    //put blur image in writeable state
    m_image_outputs[frame_index][2].image.GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

    m_blur_hor->GetPipeline()->Bind(command_buffer);

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_blur_hor->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0,
        FixedArray { static_cast<UInt32>(Engine::Get()->GetRenderState().GetScene().id.ToIndex() * sizeof(SceneShaderData))}
    );

    m_blur_hor->GetPipeline()->Dispatch(command_buffer, Extent3D(m_extent) / Extent3D { 8, 8, 1 });

    // transition blur image back into read state
    m_image_outputs[frame_index][2].image.GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);


    // PASS 4 - blur image vertically

    //put blur image in writeable state
    m_image_outputs[frame_index][3].image.GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

    m_blur_vert->GetPipeline()->Bind(command_buffer);

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_blur_vert->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0,
        FixedArray { UInt32(Engine::Get()->GetRenderState().GetScene().id.ToIndex() * sizeof(SceneShaderData))}
    );

    m_blur_vert->GetPipeline()->Dispatch(command_buffer, Extent3D(m_extent) / Extent3D { 8, 8, 1 });

    // transition blur image back into read state
    m_image_outputs[frame_index][3].image.GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    if (use_temporal_blending) {
        m_temporal_blending.Render(frame);
    }

    m_is_rendered = true;
    /* ==========  END SSR  ========== */
}

} // namespace hyperion::v2
