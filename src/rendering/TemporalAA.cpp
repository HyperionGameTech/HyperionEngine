#include <rendering/TemporalAA.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <math/Vector2.hpp>
#include <math/MathUtil.hpp>
#include <math/Halton.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Extent3D;
using renderer::ImageDescriptor;
using renderer::StorageImageDescriptor;
using renderer::SamplerDescriptor;
using renderer::UniformBufferDescriptor;
using renderer::DynamicStorageBufferDescriptor;
using renderer::ShaderVec2;
using renderer::ShaderMat4;

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

void TemporalAA::Create(Engine *engine)
{
    CreateImages(engine);
    CreateDescriptorSets(engine);
    CreateComputePipelines(engine);
}

void TemporalAA::Destroy(Engine *engine)
{
    m_compute_taa.Reset();

    // release our owned descriptor sets
    for (auto &descriptor_set : m_descriptor_sets) {
        engine->SafeRelease(std::move(descriptor_set));
    }
    
    for (auto &uniform_buffer : m_uniform_buffers) {
        engine->SafeRelease(std::move(uniform_buffer));
    }

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        auto result = Result::OK;

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            m_image_outputs[frame_index].Destroy(engine->GetDevice());

            // unset final result from the global descriptor set
            auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::TEMPORAL_AA_RESULT)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &engine->GetPlaceholderData().GetImageView2D1x1R8()
                });
        }

        return result;
    });

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void TemporalAA::CreateImages(Engine *engine)
{
    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (auto &image_output : m_image_outputs) {
            HYPERION_BUBBLE_ERRORS(image_output.Create(engine->GetDevice()));
        }

        HYPERION_RETURN_OK;
    });
}

void TemporalAA::CreateDescriptorSets(Engine *engine)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = UniquePtr<DescriptorSet>::Construct();

        // input 0 - current frame being rendered
        descriptor_set->GetOrAddDescriptor<ImageDescriptor>(0)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &engine->GetDeferredRenderer()
                    .GetCombinedResult(frame_index)->GetImageView()
            });

        // input 1 - previous frame
        descriptor_set->GetOrAddDescriptor<ImageDescriptor>(1)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_image_outputs[(frame_index + 1) % max_frames_in_flight].image_view,
               // &engine->GetDeferredRenderer()
               //     .GetCombinedResult((frame_index + 1) % max_frames_in_flight)->GetImageView()
            });

        // input 2 - velocity
        descriptor_set->GetOrAddDescriptor<ImageDescriptor>(2)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView()
            });

        // gbuffer input - depth
        descriptor_set->GetOrAddDescriptor<ImageDescriptor>(3)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView()
            });
        
        // linear sampler
        descriptor_set->GetOrAddDescriptor<SamplerDescriptor>(4)
            ->SetSubDescriptor({
                .element_index = 0u,
                .sampler = &engine->GetPlaceholderData().GetSamplerLinear()
            });
        
        // nearest sampler
        descriptor_set->GetOrAddDescriptor<SamplerDescriptor>(5)
            ->SetSubDescriptor({
                .element_index = 0u,
                .sampler = &engine->GetPlaceholderData().GetSamplerNearest()
            });

        // output
        descriptor_set->GetOrAddDescriptor<StorageImageDescriptor>(6)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_image_outputs[frame_index].image_view
            });

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }
    
    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            // create our own descriptor sets
            AssertThrow(m_descriptor_sets[frame_index] != nullptr);
            
            HYPERION_BUBBLE_ERRORS(m_descriptor_sets[frame_index]->Create(
                engine->GetDevice(),
                &engine->GetInstance()->GetDescriptorPool()
            ));

            // Add the final result to the global descriptor set
            auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::TEMPORAL_AA_RESULT)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &m_image_outputs[frame_index].image_view
                });
        }

        HYPERION_RETURN_OK;
    });
}

void TemporalAA::CreateComputePipelines(Engine *engine)
{
    m_compute_taa = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("TemporalAA")),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    engine->InitObject(m_compute_taa);
}

void TemporalAA::Render(
    Engine *engine,
    Frame *frame
)
{
    const auto &scene = engine->GetRenderState().GetScene().scene;

    m_image_outputs[frame->GetFrameIndex()].image.GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    const auto &extent = m_image_outputs[frame->GetFrameIndex()].image.GetExtent();

    struct alignas(128) {
        ShaderVec2<UInt32> dimension;
    } push_constants;

    push_constants.dimension = Extent2D(extent);

    m_compute_taa->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
    m_compute_taa->GetPipeline()->Bind(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        engine->GetInstance()->GetDescriptorPool(),
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