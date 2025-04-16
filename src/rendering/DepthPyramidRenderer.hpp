/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DEPTH_PYRAMID_RENDERER_HPP
#define HYPERION_DEPTH_PYRAMID_RENDERER_HPP

#include <core/containers/Array.hpp>

#include <core/functional/Delegate.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/Extent.hpp>

namespace hyperion {

class DepthPyramidRenderer
{
public:
    DepthPyramidRenderer();
    ~DepthPyramidRenderer();

    HYP_FORCE_INLINE const ImageViewRef &GetResultImageView() const
        { return m_depth_pyramid_view; }

    HYP_FORCE_INLINE bool IsRendered() const
        { return m_is_rendered; }

    Vec2u GetExtent() const;

    void Create();

    void Render(IFrame *frame);

private:
    AttachmentRef               m_depth_attachment;

    ImageRef                    m_depth_pyramid;
    ImageViewRef                m_depth_pyramid_view;
    Array<ImageViewRef>         m_mip_image_views;
    Array<GPUBufferRef>         m_mip_uniform_buffers;
    Array<DescriptorTableRef>   m_mip_descriptor_tables;
    SamplerRef                  m_depth_pyramid_sampler;

    ComputePipelineRef          m_generate_depth_pyramid;

    DelegateHandler             m_create_depth_pyramid_resources_handler;

    bool                        m_is_rendered;
};

} // namespace hyperion

#endif