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
using renderer::AttachmentUsage;
;
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

    void Render(Frame *frame);

private:
    void CreateImages();
    void CreateDescriptorSets();
    void CreateRaytracingPipeline();
    void CreateTemporalBlending();
    void UpdateUniforms(Frame *frame);
    void SubmitPushConstants(CommandBuffer *command_buffer);
    
    struct ImageOutput
    {
        ImageRef image;
        ImageViewRef image_view;

        ImageOutput(StorageImage &&image)
            : image(RenderObjects::Make<Image>(std::move(image))),
              image_view(RenderObjects::Make<ImageView>())
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
    };

    Extent2D m_extent;
    Handle<TLAS> m_tlas;
    
    FixedArray<UInt32, max_frames_in_flight> m_updates;

    Handle<Shader> m_shader;

    FixedArray<ImageOutput, max_frames_in_flight> m_image_outputs;
    UniquePtr<TemporalBlending> m_temporal_blending;

    RaytracingPipelineRef m_raytracing_pipeline;
    FixedArray<DescriptorSetRef, max_frames_in_flight> m_descriptor_sets;
};

} // namespace hyperion::v2

#endif