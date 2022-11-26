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

struct RENDER_COMMAND(CreateTemporalBlendingImageOutputs) : RenderCommandBase2
{
    TemporalBlending::ImageOutput *image_outputs;

    RENDER_COMMAND(CreateTemporalBlendingImageOutputs)(TemporalBlending::ImageOutput *image_outputs)
        : image_outputs(image_outputs)
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

struct RENDER_COMMAND(DestroyTemporalBlendingImageOutputs) : RenderCommandBase2
{
    TemporalBlending::ImageOutput *image_outputs;

    RENDER_COMMAND(DestroyTemporalBlendingImageOutputs)(TemporalBlending::ImageOutput *image_outputs)
        : image_outputs(image_outputs)
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            image_outputs[frame_index].Destroy(Engine::Get()->GetGPUDevice());
        }

        return result;
    }
};

struct RENDER_COMMAND(CreateTemporalBlendingDescriptors) : RenderCommandBase2
{
    UniquePtr<renderer::DescriptorSet> *descriptor_sets;

    RENDER_COMMAND(CreateTemporalBlendingDescriptors)(UniquePtr<renderer::DescriptorSet> *descriptor_sets)
        : descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto &descriptor_set = descriptor_sets[frame_index];
            AssertThrow(descriptor_set != nullptr);
                
            HYPERION_ASSERT_RESULT(descriptor_set->Create(
                Engine::Get()->GetGPUDevice(),
                &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
            ));
        }

        HYPERION_RETURN_OK;
    }
};


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

void TemporalBlending::Create()
{
    CreateImageOutputs();
    CreateDescriptorSets();
    CreateComputePipelines();
}

void TemporalBlending::Destroy()
{
    m_perform_blending.Reset();

    for (auto &descriptor_set : m_descriptor_sets) {
        Engine::Get()->SafeRelease(std::move(descriptor_set));
    }

    RenderCommands::Push<RENDER_COMMAND(DestroyTemporalBlendingImageOutputs)>(
        m_image_outputs.Data()
    );
}

void TemporalBlending::CreateImageOutputs()
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

    RenderCommands::Push<RENDER_COMMAND(CreateTemporalBlendingImageOutputs)>(
        m_image_outputs.Data()
    );
}

void TemporalBlending::CreateDescriptorSets()
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
                .image_view = Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_VELOCITY)->GetImageView()
            });

        // sampler to use
        descriptor_set
            ->AddDescriptor<SamplerDescriptor>(3)
            ->SetSubDescriptor({
                .sampler = &Engine::Get()->GetPlaceholderData().GetSamplerLinear()
            });

        // sampler to use
        descriptor_set
            ->AddDescriptor<SamplerDescriptor>(4)
            ->SetSubDescriptor({
                .sampler = &Engine::Get()->GetPlaceholderData().GetSamplerNearest()
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
                .buffer = Engine::Get()->shader_globals->scenes.GetBuffers()[frame_index].get(),
                .range = static_cast<UInt>(sizeof(SceneShaderData))
            });

        // depth texture
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(7)
            ->SetSubDescriptor({
                .image_view = Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
                    .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView()
            });

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    RenderCommands::Push<RENDER_COMMAND(CreateTemporalBlendingDescriptors)>(
        m_descriptor_sets.Data()
    );
}

void TemporalBlending::CreateComputePipelines()
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

    m_perform_blending = Engine::Get()->CreateObject<ComputePipeline>(
        Engine::Get()->CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("TemporalBlending")),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    Engine::Get()->InitObject(m_perform_blending);
}

void TemporalBlending::Render(
    
    Frame *frame
)
{
    const auto &scene_binding = Engine::Get()->render_state.GetScene();
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
        Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE)
            .GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetAttachment()->GetImage()->GetExtent()
    );

    m_perform_blending->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
    m_perform_blending->GetPipeline()->Bind(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
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
