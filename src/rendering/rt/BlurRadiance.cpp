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

BlurRadiance::BlurRadiance(
    const Extent2D &extent,
    Image *radiance_image,
    ImageView *radiance_image_view,
    ImageView *data_image_view,
    ImageView *depth_image_view
) : m_extent(extent),
    m_radiance_image(radiance_image),
    m_radiance_image_view(radiance_image_view),
    m_data_image_view(data_image_view),
    m_depth_image_view(depth_image_view)
{
}

BlurRadiance::~BlurRadiance()
{
}

void BlurRadiance::Create(Engine *engine)
{
    CreateImageOutputs(engine);
    CreateDescriptorSets(engine);
    CreateComputePipelines(engine);
}

void BlurRadiance::Destroy(Engine *engine)
{
    m_blur_hor.Reset();
    m_blur_vert.Reset();

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        auto result = Result::OK;

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (auto &image_output : m_image_outputs[frame_index]) {
                image_output.Destroy(engine->GetDevice());
            }

            if constexpr (!generate_mipmap) {
                // destroy our descriptor sets
                for (auto &descriptor_set : m_descriptor_sets[frame_index]) {
                    HYPERION_PASS_ERRORS(
                        descriptor_set->Destroy(engine->GetDevice()),
                        result
                    );
                }
            }
        }

        return result;
    });
}

void BlurRadiance::CreateImageOutputs(Engine *engine)
{
    static const FixedArray image_formats {
        InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8
    };
    
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        for (UInt i = 0; i < m_image_outputs[frame_index].Size(); i++) {
            auto &image_output = m_image_outputs[frame_index][i];

            image_output.image = StorageImage(
                Extent3D(m_extent),
                image_formats[i],
                ImageType::TEXTURE_TYPE_2D,
                FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
                nullptr
            );
        }
    }

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (UInt i = 0; i < m_image_outputs[frame_index].Size(); i++) {
                m_image_outputs[frame_index][i].Create(engine->GetDevice());
            }
        }

        HYPERION_RETURN_OK;
    });
}

void BlurRadiance::CreateDescriptorSets(Engine *engine)
{
    if constexpr (generate_mipmap) {
        return;
    }

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        for (UInt i = 0; i < m_descriptor_sets[frame_index].Size(); i++) {
            auto descriptor_set = UniquePtr<DescriptorSet>::Construct();

            // radiance image
            descriptor_set
                ->AddDescriptor<StorageImageDescriptor>(0)
                ->SetSubDescriptor({
                    .image_view = m_radiance_image_view
                });

            // data image
            descriptor_set
                ->AddDescriptor<StorageImageDescriptor>(1)
                ->SetSubDescriptor({
                    .image_view = m_data_image_view
                });

            // depth image
            descriptor_set
                ->AddDescriptor<StorageImageDescriptor>(2)
                ->SetSubDescriptor({
                    .image_view = m_depth_image_view
                });

            // input image (first pass just radiance image, second pass is prev image)
            descriptor_set
                ->AddDescriptor<ImageDescriptor>(3)
                ->SetSubDescriptor({
                    .image_view = i == 0
                        ? m_radiance_image_view
                        : &m_image_outputs[frame_index][i - 1].image_view
                });

            // sampler to use
            descriptor_set
                ->AddDescriptor<SamplerDescriptor>(4)
                ->SetSubDescriptor({
                    .sampler = &engine->GetPlaceholderData().GetSamplerLinear()
                });

            // blurred output
            descriptor_set
                ->AddDescriptor<StorageImageDescriptor>(5)
                ->SetSubDescriptor({
                    .image_view = &m_image_outputs[frame_index][i].image_view
                });

            // scene buffer - so we can reconstruct world positions
            descriptor_set
                ->AddDescriptor<DynamicStorageBufferDescriptor>(10)
                ->SetSubDescriptor({
                    .buffer = engine->shader_globals->scenes.GetBuffers()[frame_index].get(),
                    .range = static_cast<UInt>(sizeof(SceneShaderData))
                });

            m_descriptor_sets[frame_index][i] = std::move(descriptor_set);
        }
    }

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (auto &descriptor_set : m_descriptor_sets[frame_index]) {
                AssertThrow(descriptor_set != nullptr);
                
                HYPERION_ASSERT_RESULT(descriptor_set->Create(
                    engine->GetDevice(),
                    &engine->GetInstance()->GetDescriptorPool()
                ));
            }
        }

        HYPERION_RETURN_OK;
    });
}

void BlurRadiance::CreateComputePipelines(Engine *engine)
{
    if constexpr (generate_mipmap) {
        return;
    }

    m_blur_hor = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "vkshaders/rt/blur/BlurRadianceHor.comp.spv")).Read()}}
            }
        ),
        DynArray<const DescriptorSet *> { m_descriptor_sets[0][0].Get() }
    );

    engine->InitObject(m_blur_hor);

    m_blur_vert = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "vkshaders/rt/blur/BlurRadianceVert.comp.spv")).Read()}}
            }
        ),
        DynArray<const DescriptorSet *> { m_descriptor_sets[0][0].Get() }
    );

    engine->InitObject(m_blur_vert);
}

void BlurRadiance::Render(
    Engine *engine,
    Frame *frame
)
{
    if constexpr (generate_mipmap) {
        // assume render thread
        auto *src_image = m_radiance_image;
        auto &dst_image = m_image_outputs[frame->GetFrameIndex()][0].image;

        // generate mips
        // put src image in state for copying from
        src_image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_SRC);
        // put dst image in state for copying to
        dst_image.GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_DST);

        // Blit into the mipmap chain img
        dst_image.Blit(
            frame->GetCommandBuffer(),
            src_image,
            Rect { 0, 0, src_image->GetExtent().width, src_image->GetExtent().height },
            Rect { 0, 0, dst_image.GetExtent().width, dst_image.GetExtent().height }
        );

        HYPERION_ASSERT_RESULT(dst_image.GenerateMipmaps(
            engine->GetDevice(),
            frame->GetCommandBuffer()
        ));

        src_image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    } else {
        auto &descriptor_sets = m_descriptor_sets[frame->GetFrameIndex()];
        FixedArray passes { m_blur_hor.Get(), m_blur_vert.Get() };
        
        const auto scene_binding = engine->render_state.GetScene();
        const auto scene_id = scene_binding.id;
        const UInt scene_index = scene_id ? scene_id.value - 1 : 0u;

        for (UInt i = 0; i < static_cast<UInt>(passes.Size()); i++) {
            auto *pass = passes[i];

            pass->GetPipeline()->Bind(frame->GetCommandBuffer());

            frame->GetCommandBuffer()->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                pass->GetPipeline(),
                descriptor_sets[i].Get(),
                0,
                FixedArray { static_cast<UInt32>(scene_index * sizeof(SceneShaderData)) }
            );

            const auto &extent = m_image_outputs[frame->GetFrameIndex()][i].image.GetExtent();

            pass->GetPipeline()->Dispatch(
                frame->GetCommandBuffer(),
                Extent3D {
                    (extent.width + 7) / 8,
                    (extent.height + 7) / 8,
                    (extent.depth + 7) / 8
                }
            );

            // set it to be able to be used as texture2D for next pass, or outside of this
            m_image_outputs[frame->GetFrameIndex()][i].image.GetGPUImage()
                ->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
        }
    }
}

} // namespace hyperion::v2
