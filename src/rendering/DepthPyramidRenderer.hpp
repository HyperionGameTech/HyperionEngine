/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DEPTH_PYRAMID_RENDERER_HPP
#define HYPERION_DEPTH_PYRAMID_RENDERER_HPP

#include <core/Containers.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>

namespace hyperion {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;
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

    void Create(AttachmentUsageRef depth_attachment_usage);
    void Destroy();

    void Render(Frame *frame);

private:
    AttachmentUsageRef                                          m_depth_attachment_usage;

    ImageRef                                                    m_depth_pyramid;
    ImageViewRef                                                m_depth_pyramid_view;
    Array<ImageViewRef>                                         m_depth_pyramid_mips;
    Array<DescriptorTableRef>                                   m_mip_descriptor_tables;
    SamplerRef                                                  m_depth_pyramid_sampler;

    ComputePipelineRef                                          m_generate_depth_pyramid;

    bool m_is_rendered;
};

} // namespace hyperion

#endif