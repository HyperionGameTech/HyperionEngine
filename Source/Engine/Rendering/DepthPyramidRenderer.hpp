/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Rendering/RenderTypes.hpp>

#include <Core/Math/Extent.hpp>

namespace Hyperion {

class GBuffer;

class DepthPyramidRenderer
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    explicit DepthPyramidRenderer(GBuffer* gbuffer);
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

    HYP_FORCE_INLINE uint32 GetTotalMips() const
    {
        return uint32(m_mipImageViews.Size());
    }

    void Create();

    void Render(Frame* frame);

private:
    GBuffer* m_gbuffer;

    GpuImageViewRef m_depthImageView;
    GpuImageRef m_depthPyramid;
    GpuImageViewRef m_depthPyramidView;

    Array<GpuImageViewRef, RenderAllocator> m_mipImageViews;
    Array<GpuBufferRef, RenderAllocator> m_mipUniformBuffers;

    bool m_isRendered;
};

} // namespace Hyperion
