#ifndef HYPERION_V2_RT_BLUR_RADIANCE_HPP
#define HYPERION_V2_RT_BLUR_RADIANCE_HPP

#include <Constants.hpp>

#include <core/Containers.hpp>

#include <rendering/Compute.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;
using renderer::DescriptorSet;
using renderer::AttachmentRef;
using renderer::Extent2D;

class Engine;

class BlurRadiance
{
    static constexpr bool generate_mipmap = false;

public:
    struct ImageOutput
    {
        StorageImage image;
        ImageView image_view;

        void Create(Device *device)
        {
            HYPERION_ASSERT_RESULT(image.Create(device));
            HYPERION_ASSERT_RESULT(image_view.Create(device, &image));
        }

        void Destroy(Device *device)
        {
            HYPERION_ASSERT_RESULT(image.Destroy(device));
            HYPERION_ASSERT_RESULT(image_view.Destroy(device));
        }
    };

    BlurRadiance(
        const Extent2D &extent,
        Image *radiance_image,
        ImageView *radiance_image_view,
        ImageView *data_image_view,
        ImageView *depth_image_view
    );
    ~BlurRadiance();

    ImageOutput &GetImageOutput(UInt frame_index)
        { return m_image_outputs[frame_index].Back(); }

    const ImageOutput &GetImageOutput(UInt frame_index) const
        { return m_image_outputs[frame_index].Back(); }

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void Render(
        Engine *engine,
        Frame *frame
    );

private:
    void CreateImageOutputs(Engine *engine);
    void CreateDescriptorSets(Engine *engine);
    void CreateComputePipelines(Engine *engine);

    Extent2D m_extent;
    Image *m_radiance_image;
    ImageView *m_radiance_image_view;
    ImageView *m_data_image_view;
    ImageView *m_depth_image_view;

    FixedArray<FixedArray<UniquePtr<DescriptorSet>, 2>, max_frames_in_flight> m_descriptor_sets;
    FixedArray<FixedArray<ImageOutput, generate_mipmap ? 1 : 2>, max_frames_in_flight> m_image_outputs;

    Handle<ComputePipeline> m_write_uvs;
    Handle<ComputePipeline> m_sample;
    Handle<ComputePipeline> m_blur_hor;
    Handle<ComputePipeline> m_blur_vert;
};

} // namespace hyperion::v2

#endif