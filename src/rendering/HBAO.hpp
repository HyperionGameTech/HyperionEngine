#ifndef HYPERION_V2_HBAO_HPP
#define HYPERION_V2_HBAO_HPP

#include <Constants.hpp>
#include <core/Containers.hpp>

#include <rendering/TemporalBlending.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/Compute.hpp>

#include <array>

namespace hyperion::v2 {

using renderer::StorageImage;
using renderer::ImageView;
using renderer::Frame;
;
using renderer::Result;

class Engine;

struct RenderCommand_AddHBAOFinalImagesToGlobalDescriptorSet;

class HBAO
{
    static constexpr bool blur_result = false;

public:
    friend struct RenderCommand_AddHBAOFinalImagesToGlobalDescriptorSet;

    HBAO(const Extent2D &extent);
    HBAO(const HBAO &other) = delete;
    HBAO &operator=(const HBAO &other) = delete;
    ~HBAO();

    void Create();
    void Destroy();
    
    void Render(Frame *frame);

private:
    void CreateImages();
    void CreatePass();
    void CreateTemporalBlending();
    void CreateDescriptorSets();
    void CreateBlurComputeShaders();
    
    struct ImageOutput
    {
        ImageRef image;
        ImageViewRef image_view;

        Result Create(Device *device);
    };

    Extent2D m_extent;

    FixedArray<FixedArray<ImageOutput, 2>, max_frames_in_flight> m_blur_image_outputs;

    FixedArray<DescriptorSetRef, max_frames_in_flight> m_descriptor_sets;
    FixedArray<FixedArray<DescriptorSetRef, max_frames_in_flight>, 2> m_blur_descriptor_sets;

    UniquePtr<FullScreenPass> m_hbao_pass;
    Handle<ComputePipeline> m_blur_hor;
    Handle<ComputePipeline> m_blur_vert;
    UniquePtr<TemporalBlending> m_temporal_blending;
};

} // namespace hyperion::v2

#endif