#ifndef HYPERION_V2_RT_RADIANCE_RENDERER_HPP
#define HYPERION_V2_RT_RADIANCE_RENDERER_HPP

#include <Constants.hpp>

#include <core/Containers.hpp>

#include <rendering/Compute.hpp>
#include <rendering/rt/BlurRadiance.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::StorageImage;
using renderer::Sampler;
using renderer::Device;
using renderer::DescriptorSet;
using renderer::AttachmentRef;
using renderer::Extent2D;
using renderer::Result;
using renderer::RaytracingPipeline;

class Engine;

class RTRadianceRenderer
{
public:
    RTRadianceRenderer(const Extent2D &extent);
    ~RTRadianceRenderer();

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void Render(
        Engine *engine,
        Frame *frame
    );

private:
    void CreateImages(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void CreateRaytracingPipeline(Engine *engine);
    void CreateBlurRadiance(Engine *engine);
    
    struct ImageOutput
    {
        StorageImage image;
        ImageView image_view;

        ImageOutput(StorageImage &&image)
            : image(std::move(image))
        {
        }

        ImageOutput(const ImageOutput &other) = delete;
        ImageOutput(ImageOutput &&other) noexcept
            : image(std::move(other.image)),
              image_view(std::move(other.image_view))
        {
        }

        ~ImageOutput() = default;

        Result Create(Device *device);
        Result Destroy(Device *device);
    };

    Extent2D m_extent;

    FixedArray<ImageOutput, 3> m_image_outputs;
    BlurRadiance m_blur_radiance;

    UniquePtr<RaytracingPipeline> m_raytracing_pipeline;
    //FixedArray<FixedArray<DescriptorSet *, 2>, max_frames_in_flight> m_descriptor_sets;
};

} // namespace hyperion::v2

#endif