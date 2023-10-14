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

class DepthPyramidRenderer
{
public:
    DepthPyramidRenderer();
    ~DepthPyramidRenderer();

    const ImageViewRef &GetResultImageView() const
        { return m_depth_pyramid_view; }

    const Array<ImageViewRef> &GetMipImageVIews() const
        { return m_depth_pyramid_mips; }

    Extent3D GetExtent() const
        { return m_depth_pyramid ? m_depth_pyramid->GetExtent() : Extent3D { 1, 1, 1 }; }

    bool IsRendered() const
        { return m_is_rendered; }

    void Create(const AttachmentUsage *depth_attachment_usage);
    void Destroy();

    void Render(Frame *frame);

private:
    const AttachmentUsage                                       *m_depth_attachment_usage;

    ImageRef                                                    m_depth_pyramid;
    ImageViewRef                                                m_depth_pyramid_view;
    Array<ImageViewRef>                                         m_depth_pyramid_mips;
    FixedArray<Array<DescriptorSetRef>, max_frames_in_flight>   m_depth_pyramid_descriptor_sets;
    SamplerRef                                                  m_depth_pyramid_sampler;

    Handle<ComputePipeline> m_generate_depth_pyramid;

    bool m_is_rendered;
};

} // namespace hyperion::v2

#endif