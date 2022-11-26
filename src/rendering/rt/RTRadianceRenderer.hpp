#ifndef HYPERION_V2_RT_RADIANCE_RENDERER_HPP
#define HYPERION_V2_RT_RADIANCE_RENDERER_HPP

#include <Constants.hpp>

#include <core/Containers.hpp>

#include <rendering/Compute.hpp>
#include <rendering/Shader.hpp>
#include <rendering/TemporalBlending.hpp>
#include <rendering/rt/TLAS.hpp>

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
using renderer::RTUpdateStateFlags;

class Engine;

struct RenderCommand_DestroyRTRadianceRenderer;
struct RenderCommand_CreateRTRadianceImageOutputs;

class RTRadianceRenderer
{
public:
    friend struct RenderCommand_DestroyRTRadianceRenderer;
    friend struct RenderCommand_CreateRTRadianceImageOutputs;
 
    RTRadianceRenderer(const Extent2D &extent);
    ~RTRadianceRenderer();

    void SetTLAS(const Handle<TLAS> &tlas)
        { m_tlas = tlas; }

    void SetTLAS(Handle<TLAS> &&tlas)
        { m_tlas = std::move(tlas); }

    void ApplyTLASUpdates(RTUpdateStateFlags flags);

    void Create();
    void Destroy();

    void Render(
        
        Frame *frame
    );

private:
    void CreateImages();
    void CreateDescriptorSets();
    void CreateRaytracingPipeline();
    void CreateTemporalBlending();
    
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
    Handle<TLAS> m_tlas;

    Handle<Shader> m_shader;

    FixedArray<ImageOutput, max_frames_in_flight> m_image_outputs;
    TemporalBlending m_temporal_blending;

    UniquePtr<RaytracingPipeline> m_raytracing_pipeline;
    FixedArray<UniquePtr<DescriptorSet>, max_frames_in_flight> m_descriptor_sets;
    FixedArray<bool, max_frames_in_flight> m_has_tlas_updates;
};

} // namespace hyperion::v2

#endif