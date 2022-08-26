#include "DepthPyramidRenderer.hpp"
#include "../Engine.hpp"

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;

DepthPyramidRenderer::DepthPyramidRenderer()
    : m_depth_attachment_ref(nullptr),
      m_is_rendered(false)
{
}

DepthPyramidRenderer::~DepthPyramidRenderer()
{
}

void DepthPyramidRenderer::Create(Engine *engine, const AttachmentRef *depth_attachment_ref)
{
    AssertThrow(m_depth_attachment_ref == nullptr);
    // AssertThrow(depth_attachment_ref->IsDepthAttachment());
    m_depth_attachment_ref = depth_attachment_ref->IncRef(HYP_ATTACHMENT_REF_INSTANCE);

    m_depth_pyramid_sampler = std::make_unique<Sampler>(
        Image::FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP,
        Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    );

    HYPERION_ASSERT_RESULT(m_depth_pyramid_sampler->Create(engine->GetDevice()));

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        const auto *depth_attachment = m_depth_attachment_ref->GetAttachment();
        AssertThrow(depth_attachment != nullptr);

        const auto *depth_image = depth_attachment->GetImage();
        AssertThrow(depth_image != nullptr);

        // create depth pyramid image
        m_depth_pyramid[i] = std::make_unique<StorageImage>(
            Extent3D {
                static_cast<UInt>(MathUtil::NextPowerOf2(depth_image->GetExtent().width)),
                static_cast<UInt>(MathUtil::NextPowerOf2(depth_image->GetExtent().height)),
                1
            },
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R32F,
            Image::Type::TEXTURE_TYPE_2D,
            Image::FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP,
            nullptr
        );

        m_depth_pyramid[i]->Create(engine->GetDevice());

        m_depth_pyramid_results[i] = std::make_unique<ImageView>();
        m_depth_pyramid_results[i]->Create(engine->GetDevice(), m_depth_pyramid[i].get());

        const auto num_mip_levels = m_depth_pyramid[i]->NumMipmaps();

        m_depth_pyramid_mips[i].Reserve(num_mip_levels);

        for (UInt mip_level = 0; mip_level < num_mip_levels; mip_level++) {
            auto mip_image_view = std::make_unique<ImageView>();

            HYPERION_ASSERT_RESULT(mip_image_view->Create(
                engine->GetDevice(),
                m_depth_pyramid[i].get(),
                mip_level, 1,
                0, m_depth_pyramid[i]->NumFaces()
            ));

            m_depth_pyramid_mips[i].PushBack(std::move(mip_image_view));

            // create descriptor sets for depth pyramid generation.
            auto depth_pyramid_descriptor_set = std::make_unique<DescriptorSet>();

            /* Depth pyramid - generated w/ compute shader */
            auto *depth_pyramid_in = depth_pyramid_descriptor_set
                ->AddDescriptor<renderer::ImageDescriptor>(0);

            if (mip_level == 0) {
                // first mip level -- input is the actual depth image
                depth_pyramid_in->SetSubDescriptor({
                    .element_index = 0,
                    .image_view    = depth_attachment_ref->GetImageView()
                });
            } else {
                depth_pyramid_in->SetSubDescriptor({
                    .element_index = 0,
                    .image_view    = m_depth_pyramid_mips[i][mip_level - 1].get()
                });
            }

            auto *depth_pyramid_out = depth_pyramid_descriptor_set
                ->AddDescriptor<renderer::StorageImageDescriptor>(1);

            depth_pyramid_out->SetSubDescriptor({
                .element_index = 0,
                .image_view    = m_depth_pyramid_mips[i][mip_level].get()
            });

            depth_pyramid_descriptor_set
                ->AddDescriptor<renderer::SamplerDescriptor>(2)
                ->SetSubDescriptor({
                    .sampler = m_depth_pyramid_sampler.get()
                });

            HYPERION_ASSERT_RESULT(depth_pyramid_descriptor_set->Create(
                engine->GetDevice(),
                &engine->GetInstance()->GetDescriptorPool()
            ));

            m_depth_pyramid_descriptor_sets[i].PushBack(std::move(depth_pyramid_descriptor_set));
        }
    }
    // create compute pipeline for rendering depth image
    m_generate_depth_pyramid = Handle<ComputePipeline>(new ComputePipeline(
        Handle<Shader>(new Shader(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/cull/generate_depth_pyramid.comp.spv")).Read()}}
            }
        )),
        DynArray<const DescriptorSet *> { m_depth_pyramid_descriptor_sets[0].Front().get() } // only need to pass first to use for layout.
    ));

    engine->Attach(m_generate_depth_pyramid);
}

void DepthPyramidRenderer::Destroy(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        for (auto &descriptor_set : m_depth_pyramid_descriptor_sets[i]) {
            HYPERION_ASSERT_RESULT(descriptor_set->Destroy(engine->GetDevice()));
        }

        m_depth_pyramid_descriptor_sets[i].Clear();

        for (auto &mip_image_view : m_depth_pyramid_mips[i]) {
            HYPERION_ASSERT_RESULT(mip_image_view->Destroy(engine->GetDevice()));
        }

        m_depth_pyramid_mips[i].Clear();

        HYPERION_ASSERT_RESULT(m_depth_pyramid_results[i]->Destroy(engine->GetDevice()));
        HYPERION_ASSERT_RESULT(m_depth_pyramid[i]->Destroy(engine->GetDevice()));
    }

    HYPERION_ASSERT_RESULT(m_depth_pyramid_sampler->Destroy(engine->GetDevice()));

    if (m_depth_attachment_ref != nullptr) {
        m_depth_attachment_ref->DecRef(HYP_ATTACHMENT_REF_INSTANCE);
        m_depth_attachment_ref = nullptr;
    }

    m_is_rendered = false;
}

void DepthPyramidRenderer::Render(Engine *engine, Frame *frame)
{
    auto *primary = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    DebugMarker marker(primary, "Depth pyramid generation");

    const auto num_depth_pyramid_mip_levels = m_depth_pyramid_mips[frame_index].Size();

    const auto &image_extent         = m_depth_attachment_ref->GetAttachment()->GetImage()->GetExtent();
    const auto &depth_pyramid_extent = m_depth_pyramid[frame_index]->GetExtent();

    UInt32 mip_width  = image_extent.width,
           mip_height = image_extent.height;

    for (UInt mip_level = 0; mip_level < num_depth_pyramid_mip_levels; mip_level++) {
        // level 0 == write just-rendered depth image into mip 0

        // put the mip into writeable state
        m_depth_pyramid[frame_index]->GetGPUImage()->InsertSubResourceBarrier(
            primary,
            renderer::ImageSubResource { .base_mip_level = mip_level },
            renderer::GPUMemory::ResourceState::UNORDERED_ACCESS
        );
        
        const auto prev_mip_width  = mip_width,
                   prev_mip_height = mip_height;

        mip_width  = MathUtil::Max(1, depth_pyramid_extent.width >> (mip_level));
        mip_height = MathUtil::Max(1, depth_pyramid_extent.height >> (mip_level));

        // bind descriptor set to compute pipeline
        primary->BindDescriptorSet(
            engine->GetInstance()->GetDescriptorPool(),
            m_generate_depth_pyramid->GetPipeline(),
            m_depth_pyramid_descriptor_sets[frame_index][mip_level].get(),
            static_cast<DescriptorSet::Index>(0)
        );

        // set push constant data for the current mip level
        m_generate_depth_pyramid->GetPipeline()->Bind(
            primary,
            Pipeline::PushConstantData {
                .depth_pyramid_data = {
                    .mip_dimensions      = renderer::ShaderVec2<UInt32>(mip_width, mip_height),
                    .prev_mip_dimensions = renderer::ShaderVec2<UInt32>(prev_mip_width, prev_mip_height),
                    .mip_level           = mip_level
                }
            }
        );
        
        // dispatch to generate this mip level
        m_generate_depth_pyramid->GetPipeline()->Dispatch(
            primary,
            Extent3D {
                (mip_width + 31)  / 32,
                (mip_height + 31) / 32,
                1
            }
        );

        // put this mip into readable state
        m_depth_pyramid[frame_index]->GetGPUImage()->InsertSubResourceBarrier(
            primary,
            renderer::ImageSubResource { .base_mip_level = mip_level },
            renderer::GPUMemory::ResourceState::SHADER_RESOURCE
        );
    }

    // all mip levels have been transitioned into this state
    m_depth_pyramid[frame_index]->GetGPUImage()->SetResourceState(
        renderer::GPUMemory::ResourceState::SHADER_RESOURCE
    );

    m_is_rendered = true;
}

} // namespace hyperion::v2
