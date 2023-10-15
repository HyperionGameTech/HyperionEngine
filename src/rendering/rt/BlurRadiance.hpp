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
#include <rendering/backend/RendererResult.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;
using renderer::DescriptorSet;
using renderer::AttachmentUsage;
;
using renderer::Result;

class Engine;

class BlurRadiance
{
public:
    struct ImageOutput
    {
        StorageImage image;
        ImageView image_view;

        Result Create(Device *device)
        {
            HYPERION_BUBBLE_ERRORS(image.Create(device));
            HYPERION_BUBBLE_ERRORS(image_view.Create(device, &image));
            HYPERION_RETURN_OK;
        }

        Result Destroy(Device *device)
        {
            auto result = Result::OK;

            HYPERION_PASS_ERRORS(image.Destroy(device), result);
            HYPERION_PASS_ERRORS(image_view.Destroy(device), result);

            return result;
        }
    };

    BlurRadiance(
        const Extent2D &extent,
        const FixedArray<Image *, max_frames_in_flight> &input_images,
        const FixedArray<ImageView *, max_frames_in_flight> &input_image_views
    );
    BlurRadiance(const BlurRadiance &other) = delete;
    BlurRadiance &operator=(const BlurRadiance &other) = delete;
    ~BlurRadiance();

    ImageOutput &GetImageOutput(UInt frame_index)
        { return m_image_outputs[frame_index].Back(); }

    const ImageOutput &GetImageOutput(UInt frame_index) const
        { return m_image_outputs[frame_index].Back(); }

    void Create();
    void Destroy();

    void Render(
        
        Frame *frame
    );

private:
    void CreateImageOutputs();
    void CreateDescriptorSets();
    void CreateComputePipelines();

    Extent2D m_extent;

    FixedArray<Image *, max_frames_in_flight> m_input_images;
    FixedArray<ImageView *, max_frames_in_flight> m_input_image_views;

    Handle<ComputePipeline> m_blur_hor;
    Handle<ComputePipeline> m_blur_vert;

    FixedArray<FixedArray<DescriptorSetRef, 2>, max_frames_in_flight> m_descriptor_sets;
    FixedArray<FixedArray<ImageOutput, 2>, max_frames_in_flight> m_image_outputs;
};

} // namespace hyperion::v2

#endif