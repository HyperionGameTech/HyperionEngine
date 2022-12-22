#ifndef HYPERION_V2_DEPTH_PYRAMID_RENDERER_H
#define HYPERION_V2_DEPTH_PYRAMID_RENDERER_H

#include "Compute.hpp"

#include <core/Containers.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererStructs.hpp>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;
using renderer::DescriptorSet;
using renderer::AttachmentUsage;
using renderer::Extent3D;

class DepthPyramidRenderer
{
public:
    DepthPyramidRenderer();
    ~DepthPyramidRenderer();

    auto &GetResults() { return m_depth_pyramid_results; }
    const auto &GetResults() const { return m_depth_pyramid; }

    auto &GetMips() { return m_depth_pyramid_mips; }
    const auto &GetMips() const { return m_depth_pyramid_mips; }

    const Extent3D &GetExtent() const { return m_depth_pyramid[0]->GetExtent(); }

    bool IsRendered() const { return m_is_rendered; }

    void Create(const AttachmentUsage *depth_attachment_usage);
    void Destroy();

    void Render(
        
        Frame *frame
    );

private:
    const AttachmentUsage *m_depth_attachment_usage;

    FixedArray<std::unique_ptr<Image>, max_frames_in_flight> m_depth_pyramid;
    FixedArray<std::unique_ptr<ImageView>, max_frames_in_flight> m_depth_pyramid_results;
    FixedArray<Array<std::unique_ptr<ImageView>>, max_frames_in_flight> m_depth_pyramid_mips;
    FixedArray<Array<std::unique_ptr<DescriptorSet>>, max_frames_in_flight> m_depth_pyramid_descriptor_sets;
    std::unique_ptr<Sampler> m_depth_pyramid_sampler;

    Handle<ComputePipeline> m_generate_depth_pyramid;

    bool m_is_rendered;
};

} // namespace hyperion::v2

#endif