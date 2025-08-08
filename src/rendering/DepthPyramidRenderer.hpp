/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>

#include <core/functional/Delegate.hpp>

#include <rendering/RenderObject.hpp>

#include <core/math/Extent.hpp>

namespace hyperion {

class GBuffer;

class DepthPyramidRenderer
{
public:
    DepthPyramidRenderer(GBuffer* gbuffer);
    ~DepthPyramidRenderer();

    HYP_FORCE_INLINE const GpuImageViewRef& GetResultImageView() const
    {
        return m_depthPyramidView;
    }

    HYP_FORCE_INLINE bool IsRendered() const
    {
        return m_isRendered;
    }

    Vec2u GetExtent() const;

    void Create();

    void Render(FrameBase* frame);

private:
    GBuffer* m_gbuffer;

    GpuImageViewRef m_depthImageView;
    GpuImageRef m_depthPyramid;
    GpuImageViewRef m_depthPyramidView;
    Array<GpuImageViewRef> m_mipImageViews;
    Array<GpuBufferRef> m_mipUniformBuffers;
    Array<DescriptorTableRef> m_mipDescriptorTables;
    SamplerRef m_depthPyramidSampler;

    ComputePipelineRef m_generateDepthPyramid;

    bool m_isRendered;
};

} // namespace hyperion
