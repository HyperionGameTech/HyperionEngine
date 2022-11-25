#include "BlurRadiance.hpp"
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
using renderer::Result;

struct RENDER_COMMAND(CreateBlurImageOuptuts) : RenderCommandBase2
{
    FixedArray<BlurRadiance::ImageOutput, 2> *image_outputs;

    RENDER_COMMAND(CreateBlurImageOuptuts)(FixedArray<BlurRadiance::ImageOutput, 2> *image_outputs)
        : image_outputs(image_outputs)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (UInt i = 0; i < 2; i++) {
                HYPERION_BUBBLE_ERRORS(image_outputs[frame_index][i].Create(Engine::Get()->GetDevice()));
            }
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyBlurImageOutputs) : RenderCommandBase2
{
    FixedArray<FixedArray<BlurRadiance::ImageOutput, 2>, max_frames_in_flight> image_outputs;

    RENDER_COMMAND(DestroyBlurImageOutputs)(FixedArray<FixedArray<BlurRadiance::ImageOutput, 2>, max_frames_in_flight> &&image_outputs)
        : image_outputs(std::move(image_outputs))
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            // destroy our images
            for (auto &image_output : image_outputs[frame_index]) {
                HYPERION_PASS_ERRORS(
                    image_output.Destroy(Engine::Get()->GetDevice()),
                    result
                );
            }
        }

        return result;
    }
};

struct RENDER_COMMAND(CreateBlurDescriptors) : RenderCommandBase2
{
    FixedArray<UniquePtr<DescriptorSet>, 2> *descriptor_sets;

    RENDER_COMMAND(CreateBlurDescriptors)(FixedArray<UniquePtr<DescriptorSet>, 2> *descriptor_sets)
        : descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (auto &descriptor_set : descriptor_sets[frame_index]) {
                AssertThrow(descriptor_set != nullptr);
                
                HYPERION_ASSERT_RESULT(descriptor_set->Create(
                    Engine::Get()->GetDevice(),
                    &Engine::Get()->GetInstance()->GetDescriptorPool()
                ));
            }
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyBlurDescriptors) : RenderCommandBase2
{
    FixedArray<FixedArray<UniquePtr<DescriptorSet>, 2>, max_frames_in_flight> descriptor_sets;

    RENDER_COMMAND(DestroyBlurDescriptors)(FixedArray<FixedArray<UniquePtr<DescriptorSet>, 2>, max_frames_in_flight> &&descriptor_sets)
        : descriptor_sets(std::move(descriptor_sets))
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            // destroy our descriptor sets
            for (auto &descriptor_set : descriptor_sets[frame_index]) {
                HYPERION_PASS_ERRORS(
                    descriptor_set->Destroy(Engine::Get()->GetDevice()),
                    result
                );
            }
        }

        return result;
    }
};

BlurRadiance::BlurRadiance(
    const Extent2D &extent,
    const FixedArray<Image *, max_frames_in_flight> &input_images,
    const FixedArray<ImageView *, max_frames_in_flight> &input_image_views
) : m_extent(extent),
    m_input_images(input_images),
    m_input_image_views(input_image_views)
{
}

BlurRadiance::~BlurRadiance() = default;

void BlurRadiance::Create()
{
    CreateImageOutputs(Engine::Get());
    CreateDescriptorSets(Engine::Get());
    CreateComputePipelines(Engine::Get());
}

void BlurRadiance::Destroy()
{
    m_blur_hor.Reset();
    m_blur_vert.Reset();

    RenderCommands::Push<RENDER_COMMAND(DestroyBlurDescriptors)>(std::move(m_descriptor_sets));
    RenderCommands::Push<RENDER_COMMAND(DestroyBlurImageOutputs)>(std::move(m_image_outputs));
}

void BlurRadiance::CreateImageOutputs()
{
    static const FixedArray image_formats {
        InternalFormat::RGBA8,
        InternalFormat::RGBA8
    };
    
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        for (UInt i = 0; i < m_image_outputs[frame_index].Size(); i++) {
            auto &image_output = m_image_outputs[frame_index][i];

            image_output.image = StorageImage(
                Extent3D(m_extent),
                image_formats[i],
                ImageType::TEXTURE_TYPE_2D,
                FilterMode::TEXTURE_FILTER_LINEAR,
                nullptr
            );
        }
    }

    RenderCommands::Push<RENDER_COMMAND(CreateBlurImageOuptuts)>(m_image_outputs.Data());
}

void BlurRadiance::CreateDescriptorSets()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        for (UInt i = 0; i < m_descriptor_sets[frame_index].Size(); i++) {
            auto descriptor_set = UniquePtr<DescriptorSet>::Construct();

            // input image (first pass just radiance image, second pass is prev image)
            descriptor_set
                ->AddDescriptor<ImageDescriptor>(0)
                ->SetSubDescriptor({
                    .image_view = i == 0
                        ? m_input_image_views[frame_index]
                        : &m_image_outputs[frame_index][i - 1].image_view
                });

            // previous image (used for temporal blending)
            descriptor_set
                ->AddDescriptor<ImageDescriptor>(1)
                ->SetSubDescriptor({
                    .image_view = &m_image_outputs[(frame_index + 1) % max_frames_in_flight][i].image_view
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
                    .image_view = &m_image_outputs[frame_index][i].image_view
                });

            // scene buffer - so we can reconstruct world positions
            descriptor_set
                ->AddDescriptor<DynamicStorageBufferDescriptor>(6)
                ->SetSubDescriptor({
                    .buffer = Engine::Get()->shader_globals->scenes.GetBuffers()[frame_index].get(),
                    .range = static_cast<UInt>(sizeof(SceneShaderData))
                });

            m_descriptor_sets[frame_index][i] = std::move(descriptor_set);
        }
    }

    RenderCommands::Push<RENDER_COMMAND(CreateBlurDescriptors)>(m_descriptor_sets.Data());
}

void BlurRadiance::CreateComputePipelines()
{
    m_blur_hor = Engine::Get()->CreateHandle<ComputePipeline>(
        Engine::Get()->CreateHandle<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("BlurRadianceHor")),
        Array<const DescriptorSet *> { m_descriptor_sets[0][0].Get() }
    );

    Engine::Get()->InitObject(m_blur_hor);

    m_blur_vert = Engine::Get()->CreateHandle<ComputePipeline>(
        Engine::Get()->CreateHandle<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("BlurRadianceVert")),
        Array<const DescriptorSet *> { m_descriptor_sets[0][0].Get() }
    );

    Engine::Get()->InitObject(m_blur_vert);
}

void BlurRadiance::Render(
    
    Frame *frame
)
{
    auto &descriptor_sets = m_descriptor_sets[frame->GetFrameIndex()];
    FixedArray passes { m_blur_hor.Get(), m_blur_vert.Get() };
    
    const auto &scene_binding = Engine::Get()->render_state.GetScene();
    const UInt scene_index = scene_binding.id.ToIndex();

    for (UInt i = 0; i < static_cast<UInt>(passes.Size()); i++) {
        m_image_outputs[frame->GetFrameIndex()][i].image.GetGPUImage()
            ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

        auto *pass = passes[i];

        pass->GetPipeline()->Bind(frame->GetCommandBuffer());

        frame->GetCommandBuffer()->BindDescriptorSet(
            Engine::Get()->GetInstance()->GetDescriptorPool(),
            pass->GetPipeline(),
            descriptor_sets[i].Get(),
            0,
            FixedArray { UInt32(scene_index * sizeof(SceneShaderData)) }
        );

        const auto &extent = m_image_outputs[frame->GetFrameIndex()][i].image.GetExtent();

        pass->GetPipeline()->Dispatch(
            frame->GetCommandBuffer(),
            Extent3D {
                (extent.width + 7) / 8,
                (extent.height + 7) / 8,
                1
            }
        );

        // set it to be able to be used as texture2D for next pass, or outside of this
        m_image_outputs[frame->GetFrameIndex()][i].image.GetGPUImage()
            ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    }
}

} // namespace hyperion::v2
