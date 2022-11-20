#include "TemporalBlending.hpp"
#include <Engine.hpp>
#include <Threads.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::StorageImageDescriptor;
using renderer::DynamicStorageBufferDescriptor;
using renderer::SamplerDescriptor;
using renderer::DescriptorKey;
using renderer::Rect;
using renderer::ShaderVec2;

TemporalBlending::TemporalBlending(
    const Extent2D &extent,
    const FixedArray<Image *, max_frames_in_flight> &input_images,
    const FixedArray<ImageView *, max_frames_in_flight> &input_image_views
) : TemporalBlending(
        extent,
        InternalFormat::RGBA8,
        input_images,
        input_image_views
    )
{
}

TemporalBlending::TemporalBlending(
    const Extent2D &extent,
    InternalFormat image_format,
    const FixedArray<Image *, max_frames_in_flight> &input_images,
    const FixedArray<ImageView *, max_frames_in_flight> &input_image_views
) : m_extent(extent),
    m_image_format(image_format),
    m_input_images(input_images),
    m_input_image_views(input_image_views)
{
}

TemporalBlending::~TemporalBlending() = default;

void TemporalBlending::Create(Engine *engine)
{
    CreateImageOutputs(engine);
    CreateDescriptorSets(engine);
    CreateComputePipelines(engine);
}

void TemporalBlending::Destroy(Engine *engine)
{
    m_perform_blending.Reset();

    for (auto &descriptor_set : m_descriptor_sets) {
        engine->SafeRelease(std::move(descriptor_set));
    }

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        auto result = Result::OK;

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            m_image_outputs[frame_index].Destroy(engine->GetDevice());
        }

        return result;
    });
}

void TemporalBlending::CreateImageOutputs(Engine *engine)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto &image_output = m_image_outputs[frame_index];

        image_output.image = StorageImage(
            Extent3D(m_extent),
            InternalFormat::RGBA8,
            ImageType::TEXTURE_TYPE_2D,
            FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
            nullptr
        );
    }

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            m_image_outputs[frame_index].Create(engine->GetDevice());
        }

        HYPERION_RETURN_OK;
    });
}

void TemporalBlending::CreateDescriptorSets(Engine *engine)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = UniquePtr<DescriptorSet>::Construct();

        // input image (first pass just radiance image, second pass is prev image)
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(0)
            ->SetSubDescriptor({
                .image_view = m_input_image_views[frame_index]
            });

        // previous image (used for temporal blending)
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(1)
            ->SetSubDescriptor({
                .image_view = &m_image_outputs[(frame_index + 1) % max_frames_in_flight].image_view
            });

        // velocity (used for temporal blending)
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(2)
            ->SetSubDescriptor({
                .image_view = engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView()
            });

        // sampler to use
        descriptor_set
            ->AddDescriptor<SamplerDescriptor>(3)
            ->SetSubDescriptor({
                .sampler = &engine->GetPlaceholderData().GetSamplerLinear()
            });

        // sampler to use
        descriptor_set
            ->AddDescriptor<SamplerDescriptor>(4)
            ->SetSubDescriptor({
                .sampler = &engine->GetPlaceholderData().GetSamplerNearest()
            });

        // blurred output
        descriptor_set
            ->AddDescriptor<StorageImageDescriptor>(5)
            ->SetSubDescriptor({
                .image_view = &m_image_outputs[frame_index].image_view
            });

        // scene buffer - so we can reconstruct world positions
        descriptor_set
            ->AddDescriptor<DynamicStorageBufferDescriptor>(6)
            ->SetSubDescriptor({
                .buffer = engine->shader_globals->scenes.GetBuffers()[frame_index].get(),
                .range = static_cast<UInt>(sizeof(SceneShaderData))
            });

        // depth texture
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(7)
            ->SetSubDescriptor({
                .image_view = engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView()
            });

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto &descriptor_set = m_descriptor_sets[frame_index];
            AssertThrow(descriptor_set != nullptr);
                
            HYPERION_ASSERT_RESULT(descriptor_set->Create(
                engine->GetDevice(),
                &engine->GetInstance()->GetDescriptorPool()
            ));
        }

        HYPERION_RETURN_OK;
    });
}

void TemporalBlending::CreateComputePipelines(Engine *engine)
{
    ShaderProps shader_props;

    switch (m_image_format) {
    case InternalFormat::RGBA8:
        shader_props.Set("OUTPUT_RGBA8");
        break;
    case InternalFormat::RGBA16F:
        shader_props.Set("OUTPUT_RGBA16F");
        break;
    case InternalFormat::RGBA32F:
        shader_props.Set("OUTPUT_RGBA32F");
        break;
    }

    m_perform_blending = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("TemporalBlending")),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    engine->InitObject(m_perform_blending);
}

void TemporalBlending::Render(
    Engine *engine,
    Frame *frame
)
{
    const auto &scene_binding = engine->render_state.GetScene();
    const UInt scene_index = scene_binding.id.ToIndex();
    
    m_image_outputs[frame->GetFrameIndex()].image.GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    const auto &extent = m_image_outputs[frame->GetFrameIndex()].image.GetExtent();

    struct alignas(128) {
        ShaderVec2<UInt32> output_dimensions;
        ShaderVec2<UInt32> depth_texture_dimensions;
    } push_constants;

    push_constants.output_dimensions = Extent2D(extent);
    push_constants.depth_texture_dimensions = Extent2D(
        engine->GetDeferredSystem().Get(BUCKET_OPAQUE)
            .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetAttachment()->GetImage()->GetExtent()
    );

    m_perform_blending->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
    m_perform_blending->GetPipeline()->Bind(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        engine->GetInstance()->GetDescriptorPool(),
        m_perform_blending->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0,
        FixedArray { UInt32(scene_index * sizeof(SceneShaderData)) }
    );

    m_perform_blending->GetPipeline()->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            (extent.width + 7) / 8,
            (extent.height + 7) / 8,
            1
        }
    );

    // set it to be able to be used as texture2D for next pass, or outside of this
    m_image_outputs[frame->GetFrameIndex()].image.GetGPUImage()
        ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
}

} // namespace hyperion::v2
