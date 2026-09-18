/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/Pass.hpp>

#include <Core/Math/Mat4f.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class CBufferAllocator;

// matches SkyVisibilityCapture in Shaders/DeferredIndirect.hlsl
struct SkyVisibilityShaderData
{
    Mat4f viewProjectionMatrix;

    // x = captured world extent in meters, y = capture depth range in meters, z = 1 when a capture is available
    Vec4f params;
};

HYP_CLASS(NoScriptBindings)
class SkyVisibilityPassData : public PassData
{
    HYP_OBJECT_BODY(SkyVisibilityPassData);

public:
    virtual ~SkyVisibilityPassData() override = default;
};

/*! \brief Draws the world's top-down sky visibility view (DynamicSkySystem) into its own depth map.
 *  Lighting reads the result to occlude sky light under canopies and overhangs. */
class SkyVisibilityPass final : public PassBase
{
public:
    SkyVisibilityPass();
    virtual ~SkyVisibilityPass() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    /*! \brief renderSetup.view must be the sky visibility view, with its RenderProxyList already being read by the caller. */
    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_isValid;
    }

    /*! \brief World space to the capture's clip space, as of the last render. */
    HYP_FORCE_INLINE const Mat4f& GetViewProjectionMatrix() const
    {
        return m_viewProjectionMatrix;
    }

    /*! \brief The depth map, or a placeholder while nothing has been captured. */
    const GpuImageViewRef& GetDepthImageView() const;

    /*! \brief Writes the capture's matrix and extents, zeroed while nothing has been captured. */
    void WriteShaderData(CBufferAllocator& cbufferAllocator) const;

protected:
    virtual PassData* CreateViewPassData(View* view, PassDataExt&) override;

private:
    Mat4f m_viewProjectionMatrix;
    GpuImageViewRef m_depthImageView;
    bool m_isValid;
};

} // namespace Hyperion
