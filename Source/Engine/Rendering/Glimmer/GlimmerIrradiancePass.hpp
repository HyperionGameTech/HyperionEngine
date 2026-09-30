/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/FullScreenPass.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class GBuffer;

// Must match glimmerParams in Shaders/DeferredIndirect.hlsl
struct GlimmerIrradianceShaderData
{
    Vec4u params; // x = 1 when the target holds Glimmer this frame, y = 1 when it holds a debug view in place of the irradiance
};

/*! \brief Evaluates Glimmer's irradiance for a GBuffer view into a target the deferred indirect pass reads. Stencil tested, so sky and
 *  lightmapped pixels (whose GI comes from the lightmap) are never shaded, and Glimmer's sampling stays out of the lighting shader. */
class GlimmerIrradiancePass final : public FullScreenPass
{
public:
    GlimmerIrradiancePass(Vec2u extent, GBuffer* gbuffer);
    virtual ~GlimmerIrradiancePass() override;

    /*! \brief What the deferred indirect pass needs to know about this frame's target. Valid after Render(). */
    HYP_FORCE_INLINE const GlimmerIrradianceShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    virtual void CreateFramebuffer() override;

    /*! \brief renderSetup.view must be the GBuffer view, and renderSetup.envProbe the sky probe if there is one. Leaves the target ready to be read by the lighting pass, drawn or not. */
    virtual void Render(Frame* frame, const RenderSetup& renderSetup) override;

private:
    GlimmerIrradianceShaderData m_shaderData;
};

} // namespace Hyperion
