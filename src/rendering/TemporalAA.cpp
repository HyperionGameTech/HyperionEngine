#include <rendering/TemporalAA.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <math/Vector2.hpp>
#include <math/MathUtil.hpp>
#include <math/Halton.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::StorageImageDescriptor;
using renderer::SamplerDescriptor;
using renderer::UniformBufferDescriptor;
using renderer::DynamicStorageBufferDescriptor;
using renderer::ShaderVec2;
using renderer::ShaderMat4;

struct RENDER_COMMAND(CreateTemporalAADescriptors) : RenderCommand
{
    UniquePtr<renderer::DescriptorSet> *descriptor_sets;
    TemporalAA::ImageOutput *image_outputs;

    RENDER_COMMAND(CreateTemporalAADescriptors)(
        UniquePtr<renderer::DescriptorSet> *descriptor_sets,
        TemporalAA::ImageOutput *image_outputs
    ) : descriptor_sets(descriptor_sets),
        image_outputs(image_outputs)
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
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::TEMPORAL_AA_RESULT)
                ->SetElementSRV(0, &image_outputs[frame_index].image_view);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyTemporalAADescriptorsAndImageOutputs) : RenderCommand
{
    UniquePtr<renderer::DescriptorSet> *descriptor_sets;
    TemporalAA::ImageOutput *image_outputs;

    RENDER_COMMAND(DestroyTemporalAADescriptorsAndImageOutputs)(
        UniquePtr<renderer::DescriptorSet> *descriptor_sets,
        TemporalAA::ImageOutput *image_outputs
    ) : descriptor_sets(descriptor_sets),
        image_outputs(image_outputs)
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
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::TEMPORAL_AA_RESULT)
                ->SetElementSRV(0, &Engine::Get()->GetPlaceholderData().GetImageView2D1x1R8());
        }

        return result;
    }
};

struct RENDER_COMMAND(CreateTemporalAAImageOutputs) : RenderCommand
{
    TemporalAA::ImageOutput *image_outputs;

    RENDER_COMMAND(CreateTemporalAAImageOutputs)(
        TemporalAA::ImageOutput *image_outputs
    ) : image_outputs(image_outputs)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_BUBBLE_ERRORS(image_outputs[frame_index].Create(Engine::Get()->GetGPUDevice()));
        }

        HYPERION_RETURN_OK;
    }
};

Result TemporalAA::ImageOutput::Create(Device *device)
{
    HYPERION_BUBBLE_ERRORS(image.Create(device));
    HYPERION_BUBBLE_ERRORS(image_view.Create(device, &image));

    HYPERION_RETURN_OK;
}

Result TemporalAA::ImageOutput::Destroy(Device *device)
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

TemporalAA::TemporalAA(const Extent2D &extent)
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
      }
{
}

TemporalAA::~TemporalAA() = default;

void TemporalAA::Create()
{
    CreateImages();
    CreateDescriptorSets();
    CreateComputePipelines();
}

void TemporalAA::Destroy()
{
    m_compute_taa.Reset();

    // release our owned descriptor sets
    for (auto &descriptor_set : m_descriptor_sets) {
        Engine::Get()->SafeRelease(std::move(descriptor_set));
    }
    
    for (auto &uniform_buffer : m_uniform_buffers) {
        Engine::Get()->SafeRelease(std::move(uniform_buffer));
    }

    RenderCommands::Push<RENDER_COMMAND(DestroyTemporalAADescriptorsAndImageOutputs)>(
        m_descriptor_sets.Data(),
        m_image_outputs.Data()
    );

    HYP_SYNC_RENDER();
}

void TemporalAA::CreateImages()
{
    RenderCommands::Push<RENDER_COMMAND(CreateTemporalAAImageOutputs)>(
        m_image_outputs.Data()
    );
}

void TemporalAA::CreateDescriptorSets()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = UniquePtr<DescriptorSet>::Construct();

        // input 0 - current frame being rendered
        descriptor_set->GetOrAddDescriptor<ImageDescriptor>(0)
            ->SetElementSRV(0, Engine::Get()->GetDeferredRenderer().GetCombinedResult()->GetImageView());

        // input 1 - previous frame
        descriptor_set->GetOrAddDescriptor<ImageDescriptor>(1)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_image_outputs[(frame_index + 1) % max_frames_in_flight].image_view
            });

        // input 2 - velocity
        descriptor_set->GetOrAddDescriptor<ImageDescriptor>(2)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView()
            });

        // gbuffer input - depth
        descriptor_set->GetOrAddDescriptor<ImageDescriptor>(3)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView()
            });
        
        // linear sampler
        descriptor_set->GetOrAddDescriptor<SamplerDescriptor>(4)
            ->SetSubDescriptor({
                .element_index = 0u,
                .sampler = &Engine::Get()->GetPlaceholderData().GetSamplerLinear()
            });
        
        // nearest sampler
        descriptor_set->GetOrAddDescriptor<SamplerDescriptor>(5)
            ->SetSubDescriptor({
                .element_index = 0u,
                .sampler = &Engine::Get()->GetPlaceholderData().GetSamplerNearest()
            });

        // output
        descriptor_set->GetOrAddDescriptor<StorageImageDescriptor>(6)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_image_outputs[frame_index].image_view
            });

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }
    
    RenderCommands::Push<RENDER_COMMAND(CreateTemporalAADescriptors)>(
        m_descriptor_sets.Data(),
        m_image_outputs.Data()
    );
}

void TemporalAA::CreateComputePipelines()
{
    m_compute_taa = CreateObject<ComputePipeline>(
        Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(TemporalAA)),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    InitObject(m_compute_taa);
}

void TemporalAA::Render(Frame *frame)
{
    const auto &scene = Engine::Get()->GetRenderState().GetScene().scene;
    const auto &camera = Engine::Get()->GetRenderState().GetCamera().camera;

    m_image_outputs[frame->GetFrameIndex()].image.GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    const Extent3D &extent = m_image_outputs[frame->GetFrameIndex()].image.GetExtent();

    struct alignas(128) {
        ShaderVec2<UInt32> dimensions;
        ShaderVec2<UInt32> depth_texture_dimensions;
        ShaderVec2<Float> camera_near_far;
    } push_constants;

    push_constants.dimensions = Extent2D(extent);
    push_constants.depth_texture_dimensions = Extent2D(
        Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
            .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetAttachment()->GetImage()->GetExtent()
    );
    push_constants.camera_near_far = Vector2(camera.clip_near, camera.clip_far);

    m_compute_taa->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
    m_compute_taa->GetPipeline()->Bind(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_compute_taa->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0
    );

    m_compute_taa->GetPipeline()->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            (extent.width + 7) / 8,
            (extent.height + 7) / 8,
            1
        }
    );
    
    m_image_outputs[frame->GetFrameIndex()].image.GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
}

} // namespace hyperion::v2