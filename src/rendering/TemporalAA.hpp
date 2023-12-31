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

using renderer::StorageImage;
using renderer::Image;
using renderer::ImageView;
using renderer::Frame;
;
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

    void Create();
    void Destroy();
    
    void Render(
        
        Frame *frame
    );

private:
    void CreateImages();
    void CreateDescriptorSets();
    void CreateComputePipelines();

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

    Extent2D                                            m_extent;

    Handle<Texture>                                     m_result_texture;
    Handle<Texture>                                     m_history_texture;

    FixedArray<GPUBufferRef, max_frames_in_flight>      m_uniform_buffers;
    FixedArray<DescriptorSetRef, max_frames_in_flight>  m_descriptor_sets;
    Handle<ComputePipeline>                             m_compute_taa;
};

} // namespace hyperion::v2

#endif