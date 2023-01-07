#include <rendering/HBAO.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Extent3D;
using renderer::ImageDescriptor;
using renderer::StorageImageDescriptor;
using renderer::SamplerDescriptor;
using renderer::DynamicStorageBufferDescriptor;
using renderer::CommandBuffer;

#pragma region Render commands

struct RENDER_COMMAND(CreateHBAODescriptorSets) : RenderCommand
{
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    RENDER_COMMAND(CreateHBAODescriptorSets)(const FixedArray<DescriptorSetRef, max_frames_in_flight> &descriptor_sets)
        : descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            // create our own descriptor sets
            AssertThrow(descriptor_sets[frame_index].IsValid());
            
            HYPERION_BUBBLE_ERRORS(descriptor_sets[frame_index]->Create(
                Engine::Get()->GetGPUDevice(),
                &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
            ));
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateFullScreenPass) : RenderCommand
{
    FullScreenPass *pass;

    RENDER_COMMAND(CreateFullScreenPass)(FullScreenPass *pass)
        : pass(pass)
    {
    }

    virtual Result operator()()
    {
        pass->Create();

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyFullScreenPass) : RenderCommand
{
    FullScreenPass *pass;

    RENDER_COMMAND(DestroyFullScreenPass)(FullScreenPass *pass)
        : pass(pass)
    {
    }

    virtual Result operator()()
    {
        pass->Destroy();

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(AddHBAOFinalImagesToGlobalDescriptorSet) : RenderCommand
{
    FullScreenPass *pass;
    TemporalBlending *temporal_blending;
    FixedArray<ImageView *, max_frames_in_flight> blur_image_views;

    RENDER_COMMAND(AddHBAOFinalImagesToGlobalDescriptorSet)(FullScreenPass *pass, TemporalBlending *temporal_blending, FixedArray<ImageView *, max_frames_in_flight> &&blur_image_views)
        : pass(pass),
          temporal_blending(temporal_blending),
          blur_image_views(std::move(blur_image_views))
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            // Add the final result to the global descriptor set
            auto *descriptor_set_globals = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::SSAO_GI_RESULT)
                ->SetElementSRV(0,
                    blur_image_views[frame_index]
                        ? blur_image_views[frame_index]
                        : (temporal_blending
                            ? temporal_blending->GetImageOutput(frame_index).image_view
                            : pass->GetAttachmentUsage(0)->GetImageView())
                );
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

Result HBAO::ImageOutput::Create(Device *device)
{
    HYPERION_BUBBLE_ERRORS(image.Create(device));
    HYPERION_BUBBLE_ERRORS(image_view.Create(device, &image));

    HYPERION_RETURN_OK;
}

Result HBAO::ImageOutput::Destroy(Device *device)
{
    auto result = Result::OK;

    HYPERION_PASS_ERRORS(
        image.Destroy(device),
        result
    );

    HYPERION_PASS_ERRORS(
        image_view.Destroy(device),
        result
    );

    return result;
}

HBAO::HBAO(const Extent2D &extent)
    : m_image_outputs {
          ImageOutput(StorageImage(
              Extent3D(extent.width, extent.height, 1),
              InternalFormat::RGBA8,
              ImageType::TEXTURE_TYPE_2D
          )),
          ImageOutput(StorageImage(
              Extent3D(extent.width, extent.height, 1),
              InternalFormat::RGBA8,
              ImageType::TEXTURE_TYPE_2D
          ))
      },
      m_blur_image_outputs {
          FixedArray<ImageOutput, 2> {
              ImageOutput(StorageImage(
                  Extent3D(extent.width, extent.height, 1),
                  InternalFormat::RGBA8,
                  ImageType::TEXTURE_TYPE_2D
              )),
              ImageOutput(StorageImage(
                  Extent3D(extent.width, extent.height, 1),
                  InternalFormat::RGBA8,
                  ImageType::TEXTURE_TYPE_2D
              ))
          },
          FixedArray<ImageOutput, 2> {
              ImageOutput(StorageImage(
                  Extent3D(extent.width, extent.height, 1),
                  InternalFormat::RGBA8,
                  ImageType::TEXTURE_TYPE_2D
              )),
              ImageOutput(StorageImage(
                  Extent3D(extent.width, extent.height, 1),
                  InternalFormat::RGBA8,
                  ImageType::TEXTURE_TYPE_2D
              ))
          }
      }
{
}

HBAO::~HBAO() = default;

void HBAO::Create()
{
    CreateImages();

    CreateDescriptorSets();

    CreatePass();

    CreateTemporalBlending();

    if constexpr (blur_result) {
        CreateBlurComputeShaders();
    }

    PUSH_RENDER_COMMAND(
        AddHBAOFinalImagesToGlobalDescriptorSet,
        m_hbao_pass.Get(),
        m_temporal_blending.Get(),
        FixedArray<ImageView *, max_frames_in_flight> {
            blur_result ? &m_blur_image_outputs[0].Back().image_view : nullptr,
            blur_result ? &m_blur_image_outputs[1].Back().image_view : nullptr
        }
    );
}

void HBAO::Destroy()
{
    m_temporal_blending->Destroy();

    m_blur_hor.Reset();
    m_blur_vert.Reset();

    m_hbao_pass->Destroy();

    SafeRelease(std::move(m_descriptor_sets));

    for (UInt blur_pass_index = 0; blur_pass_index < m_blur_descriptor_sets.Size(); blur_pass_index++) {
        SafeRelease(std::move(m_blur_descriptor_sets[blur_pass_index]));
    }

    struct RENDER_COMMAND(DestroyHBAOImageOutputs) : RenderCommand
    {
        HBAO::ImageOutput *image_outputs;

        RENDER_COMMAND(DestroyHBAOImageOutputs)(HBAO::ImageOutput *image_outputs)
            : image_outputs(image_outputs)
        {
        }

        virtual Result operator()()
        {
            auto result = Result::OK;

            for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                image_outputs[frame_index].Destroy(Engine::Get()->GetGPUDevice());

                // unset final result from the global descriptor set
                auto *descriptor_set_globals = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
                    .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

                descriptor_set_globals
                    ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::SSAO_GI_RESULT)
                    ->SetElementSRV(0, &Engine::Get()->GetPlaceholderData().GetImageView2D1x1R8());
            }

            return result;
        }
    };

    PUSH_RENDER_COMMAND(DestroyHBAOImageOutputs, m_image_outputs.Data());

    HYP_SYNC_RENDER();
}

void HBAO::CreateImages()
{
    struct RENDER_COMMAND(CreateHBAOImageOutputs) : RenderCommand
    {
        HBAO::ImageOutput *image_outputs;
        FixedArray<HBAO::ImageOutput, 2> *blur_image_outputs;

        RENDER_COMMAND(CreateHBAOImageOutputs)(HBAO::ImageOutput *image_outputs, FixedArray<HBAO::ImageOutput, 2> *blur_image_outputs)
            : image_outputs(image_outputs),
              blur_image_outputs(blur_image_outputs)
        {
        }

        virtual Result operator()()
        {
            for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                HYPERION_BUBBLE_ERRORS(image_outputs[frame_index].Create(Engine::Get()->GetGPUDevice()));

                if constexpr (blur_result) {
                    for (UInt i = 0; i < 2; i++) {
                        HYPERION_BUBBLE_ERRORS(blur_image_outputs[frame_index][i].Create(Engine::Get()->GetGPUDevice()));
                    }
                }
            }

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(CreateHBAOImageOutputs, m_image_outputs.Data(), m_blur_image_outputs.Data());
}

void HBAO::CreateDescriptorSets()
{
    // create main descriptor sets
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = RenderObjects::Make<DescriptorSet>();

        descriptor_set
            ->AddDescriptor<ImageDescriptor>(0)
            ->SetElementSRV(0, Engine::Get()->GetDeferredRenderer().GetMipChain(frame_index)->GetImageView());

        descriptor_set
            ->AddDescriptor<ImageDescriptor>(1)
            ->SetElementSRV(0, Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_NORMALS)->GetImageView());

        descriptor_set
            ->AddDescriptor<ImageDescriptor>(2)
            ->SetElementSRV(0, Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());

        // motion vectors
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(3)
            ->SetElementSRV(0, Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView());

        // material
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(4)
            ->SetElementSRV(0, Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_MATERIAL)->GetImageView());

        // nearest sampler
        descriptor_set
            ->AddDescriptor<SamplerDescriptor>(5)
            ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerNearest());

        // linear sampler
        descriptor_set
            ->AddDescriptor<SamplerDescriptor>(6)
            ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerLinear());

        descriptor_set
            ->AddDescriptor<DynamicStorageBufferDescriptor>(7)
            ->SetElementBuffer<SceneShaderData>(0, Engine::Get()->GetRenderData()->scenes.GetBuffer(frame_index).get());

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    PUSH_RENDER_COMMAND(
        CreateHBAODescriptorSets,
        m_descriptor_sets
    );
}

void HBAO::CreateBlurComputeShaders()
{
    AssertThrow(m_temporal_blending != nullptr);

    for (UInt blur_pass_index = 0; blur_pass_index < 2; blur_pass_index++) {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto descriptor_set = RenderObjects::Make<DescriptorSet>();

            descriptor_set
                ->AddDescriptor<ImageDescriptor>(0)
                ->SetElementSRV(0, blur_pass_index == 0
                    ? m_temporal_blending->GetImageOutput(frame_index).image_view
                    : &m_blur_image_outputs[blur_pass_index - 1][frame_index].image_view);

            descriptor_set
                ->AddDescriptor<ImageDescriptor>(1)
                ->SetElementSRV(0, Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());

            // nearest sampler
            descriptor_set
                ->AddDescriptor<SamplerDescriptor>(2)
                ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerNearest());

            // linear sampler
            descriptor_set
                ->AddDescriptor<SamplerDescriptor>(3)
                ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerLinear());

            // output image
            descriptor_set
                ->AddDescriptor<StorageImageDescriptor>(4)
                ->SetElementUAV(0, &m_blur_image_outputs[blur_pass_index][frame_index].image_view);

            m_blur_descriptor_sets[blur_pass_index][frame_index] = std::move(descriptor_set);
        }

        PUSH_RENDER_COMMAND(
            CreateHBAODescriptorSets,
            m_blur_descriptor_sets[blur_pass_index]
        );
    }

    m_blur_hor = CreateObject<ComputePipeline>(
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("ImageBlurCompute", ShaderProps({ "HORIZONTAL", "OUTPUT_RGBA8" }))),
        Array<const DescriptorSet *> { m_blur_descriptor_sets[0][0].Get() }
    );

    InitObject(m_blur_hor);

    m_blur_vert = CreateObject<ComputePipeline>(
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("ImageBlurCompute", ShaderProps({ "OUTPUT_RGBA8" }))),
        Array<const DescriptorSet *> { m_blur_descriptor_sets[1][0].Get() }
    );

    InitObject(m_blur_vert);
}

void HBAO::CreatePass()
{
    auto hbao_shader = Engine::Get()->CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("HBAO"));
    Engine::Get()->InitObject(hbao_shader);

    m_hbao_pass.Reset(new FullScreenPass(
        hbao_shader,
        Array<const DescriptorSet *> { m_descriptor_sets.Front().Get() },
        InternalFormat::RGBA8
    ));

    m_hbao_pass->Create();
}

void HBAO::CreateTemporalBlending()
{
    AssertThrow(m_hbao_pass != nullptr);

    m_temporal_blending.Reset(new TemporalBlending(
        m_hbao_pass->GetFramebuffer()->GetExtent(),
        InternalFormat::RGBA8,
        TemporalBlendTechnique::TECHNIQUE_2,
        m_hbao_pass->GetFramebuffer()
    ));

    m_temporal_blending->Create();
}

void HBAO::Render(Frame *frame)
{
    const UInt frame_index = frame->GetFrameIndex();
    CommandBuffer *command_buffer = frame->GetCommandBuffer();

    const Extent3D &extent = m_hbao_pass->GetAttachmentUsage(0)->GetAttachment()->GetImage()->GetExtent();

    {
        struct alignas(128) {
            ShaderVec2<UInt32> dimension;
            Float32 temporal_blending_factor;
        } push_constants;

        push_constants.dimension = Extent2D(extent);
        push_constants.temporal_blending_factor = 0.05f;

        m_hbao_pass->GetRenderGroup()->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
        m_hbao_pass->Begin(frame);
        
        m_hbao_pass->GetCommandBuffer(frame_index)->BindDescriptorSet(
            Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
            m_hbao_pass->GetRenderGroup()->GetPipeline(),
            m_descriptor_sets[frame_index].Get(),
            0,
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Scene, Engine::Get()->GetRenderState().GetScene().id.ToIndex())
            }
        );
        
        m_hbao_pass->GetQuadMesh()->Render(m_hbao_pass->GetCommandBuffer(frame_index));
        m_hbao_pass->End(frame);
    }
    
    m_temporal_blending->Render(frame);

    if constexpr (blur_result) { // apply blurring
        for (UInt blur_pass_index = 0; blur_pass_index < 2; blur_pass_index++) {
            ComputePipeline *compute_pipeline = blur_pass_index == 0
                ? m_blur_hor.Get()
                : m_blur_vert.Get();

            const auto &extent = m_blur_image_outputs[frame_index][blur_pass_index].image.GetExtent();

            struct alignas(128) {
                ShaderVec2<UInt32> depth_texture_dimensions;
                ShaderVec2<UInt32> output_image_dimensions;
            } blur_push_constants;

            blur_push_constants.depth_texture_dimensions = Extent2D(
                Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)
                    ->GetAttachment()->GetImage()->GetExtent()
            );

            blur_push_constants.output_image_dimensions = Extent2D(extent);

            compute_pipeline->GetPipeline()->SetPushConstants(
                &blur_push_constants,
                sizeof(blur_push_constants)
            );
            compute_pipeline->GetPipeline()->Bind(command_buffer);
    
            m_blur_image_outputs[frame_index][blur_pass_index].image.GetGPUImage()
                ->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);
    
            command_buffer->BindDescriptorSet(
                Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
                compute_pipeline->GetPipeline(),
                m_blur_descriptor_sets[blur_pass_index][frame_index].Get(),
                0
            );

            compute_pipeline->GetPipeline()->Dispatch(
                command_buffer,
                Extent3D {
                    (extent.width + 7) / 8,
                    (extent.height + 7) / 8,
                    1
                }
            );

            m_blur_image_outputs[blur_pass_index][frame_index].image.GetGPUImage()
                ->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
        }
    }
}

} // namespace hyperion::v2