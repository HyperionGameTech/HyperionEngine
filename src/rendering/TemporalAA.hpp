#ifndef HYPERION_V2_TEMPORAL_AA_HPP
#define HYPERION_V2_TEMPORAL_AA_HPP

#include <Constants.hpp>
#include <core/Containers.hpp>

#include <math/Matrix4.hpp>

#include <scene/Scene.hpp>

#include <rendering/RenderState.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/Compute.hpp>
#include <rendering/backend/RendererBuffer.hpp>

namespace hyperion::v2 {

using renderer::UniformBuffer;
using renderer::StorageImage;
using renderer::Image;
using renderer::ImageView;
using renderer::Frame;
using renderer::Extent2D;
using renderer::Result;

class Engine;

struct RenderCommand_CreateTemporalAADescriptors;
struct RenderCommand_DestroyTemporalAADescriptorsAndImageOutputs;
struct RenderCommand_CreateTemporalAAImageOutputs;

class TemporalAA
{
public:
    friend struct RenderCommand_CreateTemporalAADescriptors;
    friend struct RenderCommand_DestroyTemporalAADescriptorsAndImageOutputs;
    friend struct RenderCommand_CreateTemporalAAImageOutputs;

    TemporalAA(const Extent2D &extent);
    TemporalAA(const TemporalAA &other) = delete;
    TemporalAA &operator=(const TemporalAA &other) = delete;
    ~TemporalAA();

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    
    void Render(
        Engine *engine,
        Frame *frame
    );

private:
    void CreateImages(Engine *engine);
    void CreateDescriptorSets(Engine *engine);
    void CreateComputePipelines(Engine *engine);

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

    FixedArray<ImageOutput, max_frames_in_flight> m_image_outputs;
    FixedArray<UniquePtr<Image>, max_frames_in_flight> m_previous_images;
    FixedArray<UniquePtr<ImageView>, max_frames_in_flight> m_previous_image_views;
    FixedArray<UniquePtr<UniformBuffer>, max_frames_in_flight> m_uniform_buffers;
    FixedArray<UniquePtr<DescriptorSet>, max_frames_in_flight> m_descriptor_sets;

    Handle<ComputePipeline> m_compute_taa;
};

} // namespace hyperion::v2

#endif