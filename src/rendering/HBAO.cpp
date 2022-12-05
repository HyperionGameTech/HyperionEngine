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

    RENDER_COMMAND(AddHBAOFinalImagesToGlobalDescriptorSet)(FullScreenPass *pass)
        : pass(pass)
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
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = pass->GetAttachmentRef(0)->GetImageView()
                });
        }

        HYPERION_RETURN_OK;
    }
};

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
              InternalFormat::RGBA16F,
              ImageType::TEXTURE_TYPE_2D
          )),
          ImageOutput(StorageImage(
              Extent3D(extent.width, extent.height, 1),
              InternalFormat::RGBA16F,
              ImageType::TEXTURE_TYPE_2D
          ))
      },
      m_blur_image_outputs {
          std::array<ImageOutput, 2> {
              ImageOutput(StorageImage(
                  Extent3D(extent.width, extent.height, 1),
                  InternalFormat::RGBA16F,
                  ImageType::TEXTURE_TYPE_2D
              )),
              ImageOutput(StorageImage(
                  Extent3D(extent.width, extent.height, 1),
                  InternalFormat::RGBA16F,
                  ImageType::TEXTURE_TYPE_2D
              ))
          },
          std::array<ImageOutput, 2> {
              ImageOutput(StorageImage(
                  Extent3D(extent.width, extent.height, 1),
                  InternalFormat::RGBA16F,
                  ImageType::TEXTURE_TYPE_2D
              )),
              ImageOutput(StorageImage(
                  Extent3D(extent.width, extent.height, 1),
                  InternalFormat::RGBA16F,
                  ImageType::TEXTURE_TYPE_2D
              ))
          }
      },
      m_temporal_blending(
          extent,
          InternalFormat::RGBA16F,
          FixedArray<Image *, max_frames_in_flight> { &m_image_outputs[0].image, &m_image_outputs[1].image },
          FixedArray<ImageView *, max_frames_in_flight> { &m_image_outputs[0].image_view, &m_image_outputs[1].image_view }
      )
{
}

HBAO::~HBAO() = default;

void HBAO::Create()
{
    CreateImages();
    CreateTemporalBlending();
    CreateDescriptorSets();
    CreateComputePipelines();

    RenderCommands::Push<RENDER_COMMAND(AddHBAOFinalImagesToGlobalDescriptorSet)>(m_hbao_pass.Get());
}

void HBAO::Destroy()
{
    //m_temporal_blending.Destroy();

    //m_compute_hbao.Reset();
    m_blur_hor.Reset();
    m_blur_vert.Reset();

    m_hbao_pass->Destroy();

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        Engine::Get()->SafeRelease(std::move(m_descriptor_sets[frame_index]));

        for (UInt blur_pass_index = 0; blur_pass_index < 2; blur_pass_index++) {
            Engine::Get()->SafeRelease(std::move(m_blur_descriptor_sets[frame_index][blur_pass_index]));
        }
    }

    struct RENDER_COMMAND(DestroyHBAODescriptors) : RenderCommand
    {
        HBAO::ImageOutput *image_outputs;

        RENDER_COMMAND(DestroyHBAODescriptors)(HBAO::ImageOutput *image_outputs)
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

    RenderCommands::Push<RENDER_COMMAND(DestroyHBAODescriptors)>(m_image_outputs.Data());

    HYP_SYNC_RENDER();
}

void HBAO::CreateImages()
{
    struct RENDER_COMMAND(CreateHBAOImageOutputs) : RenderCommand
    {
        HBAO::ImageOutput *image_outputs;
        std::array<HBAO::ImageOutput, 2> *blur_image_outputs;

        RENDER_COMMAND(CreateHBAOImageOutputs)(HBAO::ImageOutput *image_outputs, std::array<HBAO::ImageOutput, 2> *blur_image_outputs)
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

    RenderCommands::Push<RENDER_COMMAND(CreateHBAOImageOutputs)>(m_image_outputs.Data(), m_blur_image_outputs.data());
}

void HBAO::CreateDescriptorSets()
{
    // create main descriptor sets
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = UniquePtr<DescriptorSet>::Construct();

        descriptor_set
            ->AddDescriptor<ImageDescriptor>(0)
            ->SetSubDescriptor({
                .image_view = Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_ALBEDO)->GetImageView()
            });

        descriptor_set
            ->AddDescriptor<ImageDescriptor>(1)
            ->SetSubDescriptor({
                .image_view = Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_NORMALS)->GetImageView()
            });

        descriptor_set
            ->AddDescriptor<ImageDescriptor>(2)
            ->SetSubDescriptor({
                .image_view = Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView()
            });

        // motion vectors
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(3)
            ->SetSubDescriptor({
                .image_view = Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView()
            });

        // nearest sampler
        descriptor_set
            ->AddDescriptor<SamplerDescriptor>(4)
            ->SetSubDescriptor({
                .sampler = &Engine::Get()->GetPlaceholderData().GetSamplerNearest()
            });

        // linear sampler
        descriptor_set
            ->AddDescriptor<SamplerDescriptor>(5)
            ->SetSubDescriptor({
                .sampler = &Engine::Get()->GetPlaceholderData().GetSamplerLinear()
            });

        descriptor_set
            ->AddDescriptor<DynamicStorageBufferDescriptor>(6)
            ->SetSubDescriptor({
                .buffer = Engine::Get()->GetRenderData()->scenes.GetBuffers()[frame_index].get(),
                .range = UInt32(sizeof(SceneShaderData))
            });

        // output image
        descriptor_set
            ->AddDescriptor<StorageImageDescriptor>(7)
            ->SetSubDescriptor({
                .image_view = &m_image_outputs[frame_index].image_view
            });

        // prev image
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(8)
            ->SetSubDescriptor({
                .image_view = &m_image_outputs[(frame_index + 1) % max_frames_in_flight].image_view
            });

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    if constexpr (blur_result) {
        // create blur descriptor sets
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (UInt blur_pass_index = 0; blur_pass_index < 2; blur_pass_index++) {
                auto descriptor_set = UniquePtr<DescriptorSet>::Construct();

                descriptor_set
                    ->AddDescriptor<ImageDescriptor>(0)
                    ->SetSubDescriptor({
                        .image_view = blur_pass_index == 0
                            ? &m_temporal_blending.GetImageOutput(frame_index).image_view//&m_image_outputs[frame_index].image_view
                            : &m_blur_image_outputs[frame_index][blur_pass_index - 1].image_view
                    });

                descriptor_set
                    ->AddDescriptor<ImageDescriptor>(1)
                    ->SetSubDescriptor({
                        .image_view = Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
                            .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView()
                    });

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
                    ->SetSubDescriptor({
                        .image_view = &m_blur_image_outputs[frame_index][blur_pass_index].image_view
                    });

                m_blur_descriptor_sets[frame_index][blur_pass_index] = std::move(descriptor_set);
            }
        }
    }

    struct RENDER_COMMAND(CreateHBAODescriptorSets) : RenderCommand
    {
        UniquePtr<DescriptorSet> *descriptor_sets;
        FixedArray<UniquePtr<DescriptorSet>, 2> *blur_descriptor_sets;

        RENDER_COMMAND(CreateHBAODescriptorSets)(UniquePtr<DescriptorSet> *descriptor_sets, FixedArray<UniquePtr<DescriptorSet>, 2> *blur_descriptor_sets)
            : descriptor_sets(descriptor_sets),
              blur_descriptor_sets(blur_descriptor_sets)
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

                if constexpr (blur_result) {
                    for (UInt blur_pass_index = 0; blur_pass_index < 2; blur_pass_index++) {
                        AssertThrow(blur_descriptor_sets[frame_index][blur_pass_index] != nullptr);
                        
                        HYPERION_BUBBLE_ERRORS(blur_descriptor_sets[frame_index][blur_pass_index]->Create(
                            Engine::Get()->GetGPUDevice(),
                            &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
                        ));
                    }
                }
            }

            HYPERION_RETURN_OK;
        }
    };

    RenderCommands::Push<RENDER_COMMAND(CreateHBAODescriptorSets)>(
        m_descriptor_sets.Data(),
        m_blur_descriptor_sets.Data()
    );
}

void HBAO::CreateComputePipelines()
{
    /*m_compute_hbao = CreateObject<ComputePipeline>(
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("HBAO")),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    InitObject(m_compute_hbao);*/
    
    auto hbao_shader = Engine::Get()->CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("HBAO"));
    Engine::Get()->InitObject(hbao_shader);

    m_hbao_pass.Reset(new FullScreenPass(
        std::move(hbao_shader),
        Array<const DescriptorSet *> { m_descriptor_sets.Front().Get() }
    ));
    m_hbao_pass->Create();

    if constexpr (blur_result) {
        m_blur_hor = CreateObject<ComputePipeline>(
            CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("ImageBlurCompute", ShaderProps({ "HORIZONTAL", "OUTPUT_RGBA16F" }))),
            Array<const DescriptorSet *> { m_blur_descriptor_sets[0][0].Get() }
        );

        InitObject(m_blur_hor);

        m_blur_vert = CreateObject<ComputePipeline>(
            CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("ImageBlurCompute", ShaderProps({ "OUTPUT_RGBA16F" }))),
            Array<const DescriptorSet *> { m_blur_descriptor_sets[0][1].Get() }
        );

        InitObject(m_blur_vert);
    }
}

void HBAO::CreateTemporalBlending()
{
    //m_temporal_blending.Create();
}

void HBAO::Render(Frame *frame)
{
    const UInt frame_index = frame->GetFrameIndex();
    CommandBuffer *command_buffer = frame->GetCommandBuffer();

    /*auto &prev_image = m_image_outputs[(frame_index + 1) % max_frames_in_flight].image;
    
    if (prev_image.GetGPUImage()->GetResourceState() != renderer::ResourceState::SHADER_RESOURCE) {
        prev_image.GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    }

    m_image_outputs[frame_index].image.GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

    const auto &extent = m_image_outputs[frame_index].image.GetExtent();*/

    const auto &extent = m_hbao_pass->GetAttachmentRef(0)->GetAttachment()->GetImage()->GetExtent();

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
            FixedArray { static_cast<UInt32>(Engine::Get()->GetRenderState().GetScene().id.ToIndex() * sizeof(SceneShaderData))}
        );
        
        m_hbao_pass->GetQuadMesh()->Render(m_hbao_pass->GetCommandBuffer(frame_index));
        m_hbao_pass->End(frame);


        /*m_compute_hbao->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
        m_compute_hbao->GetPipeline()->Bind(command_buffer);

        command_buffer->BindDescriptorSet(
            Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
            m_compute_hbao->GetPipeline(),
            m_descriptor_sets[frame_index].Get(),
            0,
            FixedArray { static_cast<UInt32>(Engine::Get()->GetRenderState().GetScene().id.ToIndex() * sizeof(SceneShaderData))}
        );

        m_compute_hbao->GetPipeline()->Dispatch(
            command_buffer,
            Extent3D {
                (extent.width + 7) / 8,
                (extent.height + 7) / 8,
                1
            }
        );*/
    }
    
    /*m_image_outputs[frame_index].image.GetGPUImage()
        ->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);*/

   // m_temporal_blending.Render(frame);

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
                m_blur_descriptor_sets[frame_index][blur_pass_index].Get(),
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

            m_blur_image_outputs[frame_index][blur_pass_index].image.GetGPUImage()
                ->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
        }
    }
}

} // namespace hyperion::v2