#include <rendering/HBAO.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::StorageImageDescriptor;
using renderer::SamplerDescriptor;
using renderer::DynamicStorageBufferDescriptor;
using renderer::DynamicUniformBufferDescriptor;
using renderer::CommandBuffer;

#pragma region Render commands

struct RENDER_COMMAND(CreateHBAODescriptorSets) : renderer::RenderCommand
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
                g_engine->GetGPUDevice(),
                &g_engine->GetGPUInstance()->GetDescriptorPool()
            ));
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(AddHBAOFinalImagesToGlobalDescriptorSet) : renderer::RenderCommand
{
    FixedArray<ImageViewRef, max_frames_in_flight> pass_image_views;
    FixedArray<ImageViewRef, max_frames_in_flight> blur_image_views;

    RENDER_COMMAND(AddHBAOFinalImagesToGlobalDescriptorSet)(FixedArray<ImageViewRef, max_frames_in_flight> &&pass_image_views, FixedArray<ImageViewRef, max_frames_in_flight> &&blur_image_views)
        : pass_image_views(std::move(pass_image_views)),
          blur_image_views(std::move(blur_image_views))
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            // Add the final result to the global descriptor set
            DescriptorSetRef descriptor_set_globals = g_engine->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::SSAO_GI_RESULT)
                ->SetElementSRV(0,
                    blur_image_views[frame_index]
                        ? blur_image_views[frame_index]
                        : pass_image_views[frame_index]
                );
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

Result HBAO::ImageOutput::Create(Device *device)
{
    AssertThrow(image.IsValid());
    AssertThrow(image_view.IsValid());

    HYPERION_BUBBLE_ERRORS(image->Create(device));
    HYPERION_BUBBLE_ERRORS(image_view->Create(device, image));

    HYPERION_RETURN_OK;
}

HBAO::HBAO(const Extent2D &extent)
    : m_extent(extent),
      m_blur_image_outputs {
          FixedArray<ImageOutput, 2> {
              ImageOutput {
                  MakeRenderObject<Image>(StorageImage(
                      Extent3D(extent.width, extent.height, 1),
                      InternalFormat::RGBA8,
                      ImageType::TEXTURE_TYPE_2D
                  )),
                  MakeRenderObject<ImageView>()
              },
              ImageOutput {
                  MakeRenderObject<Image>(StorageImage(
                      Extent3D(extent.width, extent.height, 1),
                      InternalFormat::RGBA8,
                      ImageType::TEXTURE_TYPE_2D
                  )),
                  MakeRenderObject<ImageView>()
              }
          },
          FixedArray<ImageOutput, 2> {
              ImageOutput {
                  MakeRenderObject<Image>(StorageImage(
                      Extent3D(extent.width, extent.height, 1),
                      InternalFormat::RGBA8,
                      ImageType::TEXTURE_TYPE_2D
                  )),
                  MakeRenderObject<ImageView>()
              },
              ImageOutput {
                  MakeRenderObject<Image>(StorageImage(
                      Extent3D(extent.width, extent.height, 1),
                      InternalFormat::RGBA8,
                      ImageType::TEXTURE_TYPE_2D
                  )),
                  MakeRenderObject<ImageView>()
              }
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
        FixedArray<ImageViewRef, max_frames_in_flight> {
            m_temporal_blending ? m_temporal_blending->GetImageOutput(0).image_view : m_hbao_pass->GetAttachmentUsage(0)->GetImageView(),
            m_temporal_blending ? m_temporal_blending->GetImageOutput(1).image_view : m_hbao_pass->GetAttachmentUsage(0)->GetImageView()
        },
        FixedArray<ImageViewRef, max_frames_in_flight> {
            blur_result ? m_blur_image_outputs[0].Back().image_view : ImageViewRef(),
            blur_result ? m_blur_image_outputs[1].Back().image_view : ImageViewRef()
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

    for (UInt blur_pass_index = 0; blur_pass_index < UInt(m_blur_descriptor_sets.Size()); blur_pass_index++) {
        SafeRelease(std::move(m_blur_descriptor_sets[blur_pass_index]));

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            SafeRelease(std::move(m_blur_image_outputs[frame_index][blur_pass_index].image));
            SafeRelease(std::move(m_blur_image_outputs[frame_index][blur_pass_index].image_view));
        }
    }

    struct RENDER_COMMAND(RemoveHBAODescriptors) : renderer::RenderCommand
    {
        RENDER_COMMAND(RemoveHBAODescriptors)()
        {
        }

        virtual Result operator()()
        {
            auto result = Result::OK;

            for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                // unset final result from the global descriptor set
                DescriptorSetRef descriptor_set_globals = g_engine->GetGPUInstance()->GetDescriptorPool()
                    .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

                descriptor_set_globals
                    ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::SSAO_GI_RESULT)
                    ->SetElementSRV(0, &g_engine->GetPlaceholderData().GetImageView2D1x1R8());
            }

            return result;
        }
    };

    PUSH_RENDER_COMMAND(RemoveHBAODescriptors);
}

void HBAO::CreateImages()
{
    struct RENDER_COMMAND(CreateHBAOImageOutputs) : renderer::RenderCommand
    {
        FixedArray<FixedArray<HBAO::ImageOutput, 2>, max_frames_in_flight> blur_image_outputs;

        RENDER_COMMAND(CreateHBAOImageOutputs)(
            const FixedArray<FixedArray<HBAO::ImageOutput, 2>, max_frames_in_flight> &blur_image_outputs
        ) : blur_image_outputs(blur_image_outputs)
        {
        }

        virtual Result operator()()
        {
            if constexpr (blur_result) {
                for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                    for (UInt i = 0; i < 2; i++) {
                        HYPERION_BUBBLE_ERRORS(blur_image_outputs[frame_index][i].Create(g_engine->GetGPUDevice()));
                    }
                }
            }

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(
        CreateHBAOImageOutputs,
        m_blur_image_outputs
    );
}

void HBAO::CreateDescriptorSets()
{
    // create main descriptor sets
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = MakeRenderObject<DescriptorSet>();

        descriptor_set
            ->AddDescriptor<ImageDescriptor>(0)
            ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_ALBEDO)->GetImageView());

        descriptor_set
            ->AddDescriptor<ImageDescriptor>(1)
            ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_NORMALS)->GetImageView());

        descriptor_set
            ->AddDescriptor<ImageDescriptor>(2)
            ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());

        // motion vectors
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(3)
            ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView());

        // material
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(4)
            ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_MATERIAL)->GetImageView());

        // nearest sampler
        descriptor_set
            ->AddDescriptor<SamplerDescriptor>(5)
            ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerNearest());

        // linear sampler
        descriptor_set
            ->AddDescriptor<SamplerDescriptor>(6)
            ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerLinear());

        // scene buffer
        descriptor_set
            ->AddDescriptor<DynamicStorageBufferDescriptor>(7)
            ->SetElementBuffer<SceneShaderData>(0, g_engine->GetRenderData()->scenes.GetBuffer());

        // camera
        descriptor_set
            ->AddDescriptor<DynamicUniformBufferDescriptor>(8)
            ->SetElementBuffer<CameraShaderData>(0, g_engine->GetRenderData()->cameras.GetBuffer());

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
            auto descriptor_set = MakeRenderObject<DescriptorSet>();

            descriptor_set
                ->AddDescriptor<ImageDescriptor>(0)
                ->SetElementSRV(0, blur_pass_index == 0
                    ? m_temporal_blending->GetImageOutput(frame_index).image_view
                    : m_blur_image_outputs[blur_pass_index - 1][frame_index].image_view);

            descriptor_set
                ->AddDescriptor<ImageDescriptor>(1)
                ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());

            // nearest sampler
            descriptor_set
                ->AddDescriptor<SamplerDescriptor>(2)
                ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerNearest());

            // linear sampler
            descriptor_set
                ->AddDescriptor<SamplerDescriptor>(3)
                ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerLinear());

            // output image
            descriptor_set
                ->AddDescriptor<StorageImageDescriptor>(4)
                ->SetElementUAV(0, m_blur_image_outputs[blur_pass_index][frame_index].image_view);

            m_blur_descriptor_sets[blur_pass_index][frame_index] = std::move(descriptor_set);
        }

        PUSH_RENDER_COMMAND(
            CreateHBAODescriptorSets,
            m_blur_descriptor_sets[blur_pass_index]
        );
    }

    m_blur_hor = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(HYP_NAME(ImageBlurCompute), ShaderProperties({ "HORIZONTAL", "OUTPUT_RGBA8" })),
        Array<DescriptorSetRef> { m_blur_descriptor_sets[0][0] }
    );

    InitObject(m_blur_hor);

    m_blur_vert = CreateObject<ComputePipeline>(
       g_shader_manager->GetOrCreate(HYP_NAME(ImageBlurCompute), ShaderProperties({ "OUTPUT_RGBA8" })),
        Array<DescriptorSetRef> { m_blur_descriptor_sets[1][0] }
    );

    InitObject(m_blur_vert);
}

void HBAO::CreatePass()
{
    ShaderProperties shader_properties;
    shader_properties.Set("HBIL_ENABLED", g_engine->GetConfig().Get(CONFIG_HBIL));

    Handle<Shader> hbao_shader = g_shader_manager->GetOrCreate(
        HYP_NAME(HBAO),
        shader_properties
    );

    g_engine->InitObject(hbao_shader);

    m_hbao_pass.Reset(new FullScreenPass(
        hbao_shader,
        Array<DescriptorSetRef> { m_descriptor_sets.Front() },
        InternalFormat::RGBA8,
        m_extent
    ));

    m_hbao_pass->Create();
}

void HBAO::CreateTemporalBlending()
{
    AssertThrow(m_hbao_pass != nullptr);

    m_temporal_blending.Reset(new TemporalBlending(
        m_hbao_pass->GetFramebuffer()->GetExtent(),
        InternalFormat::RGBA8,
        TemporalBlendTechnique::TECHNIQUE_3,
        TemporalBlendFeedback::LOW,
        m_hbao_pass->GetFramebuffer()
    ));

    m_temporal_blending->Create();
}

void HBAO::Render(Frame *frame)
{
    const UInt frame_index = frame->GetFrameIndex();
    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();

    {
        struct alignas(128) {
            ShaderVec2<UInt32> dimension;
        } push_constants;

        push_constants.dimension = m_extent;

        m_hbao_pass->GetRenderGroup()->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
        m_hbao_pass->Begin(frame);
        
        m_hbao_pass->GetCommandBuffer(frame_index)->BindDescriptorSet(
            g_engine->GetGPUInstance()->GetDescriptorPool(),
            m_hbao_pass->GetRenderGroup()->GetPipeline(),
            m_descriptor_sets[frame_index],
            0,
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
                HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
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

            AssertThrow(m_blur_image_outputs[frame_index][blur_pass_index].image.IsValid());
            const Extent3D &extent = m_blur_image_outputs[frame_index][blur_pass_index].image->GetExtent();

            struct alignas(128) {
                ShaderVec2<UInt32> depth_texture_dimensions;
                ShaderVec2<UInt32> output_image_dimensions;
            } blur_push_constants;

            blur_push_constants.depth_texture_dimensions = Extent2D(
                g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)
                    ->GetAttachment()->GetImage()->GetExtent()
            );

            blur_push_constants.output_image_dimensions = Extent2D(extent);

            compute_pipeline->GetPipeline()->SetPushConstants(
                &blur_push_constants,
                sizeof(blur_push_constants)
            );
            compute_pipeline->GetPipeline()->Bind(command_buffer);
    
            m_blur_image_outputs[frame_index][blur_pass_index].image->GetGPUImage()
                ->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);
    
            command_buffer->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                compute_pipeline->GetPipeline(),
                m_blur_descriptor_sets[blur_pass_index][frame_index],
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

            m_blur_image_outputs[blur_pass_index][frame_index].image->GetGPUImage()
                ->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
        }
    }
}

} // namespace hyperion::v2