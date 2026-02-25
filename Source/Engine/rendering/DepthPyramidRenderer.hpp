/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/Array.hpp>

#include <Core/functional/Delegate.hpp>

#include <rendering/RenderObject.hpp>

#include <Core/math/Extent.hpp>

namespace Hyperion {

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

    void Render(Frame* frame);

private:
    GBuffer* m_gbuffer;

    GpuImageViewRef m_depthImageView;
    GpuImageRef m_depthPyramid;
    GpuImageViewRef m_depthPyramidView;
    Array<GpuImageViewRef> m_mipImageViews;
    Array<GpuBufferRef> m_mipUniformBuffers;
    SamplerRef m_depthPyramidSampler;

    bool m_isRendered;
};

} // namespace Hyperion
