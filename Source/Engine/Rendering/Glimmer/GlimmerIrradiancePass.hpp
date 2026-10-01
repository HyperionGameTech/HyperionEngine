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

struct GlimmerIrradianceShaderData
{
    Vec4u params; // x = 1 when the target holds Glimmer this frame, y = 1 when it holds a debug view in place of the irradiance
};

class GlimmerIrradiancePass final : public FullScreenPass
{
public:
    GlimmerIrradiancePass(Vec2u extent, GBuffer* gbuffer);
    virtual ~GlimmerIrradiancePass() override;

    HYP_FORCE_INLINE const GlimmerIrradianceShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    virtual void CreateFramebuffer() override;

    virtual void Render(Frame* frame, const RenderSetup& renderSetup) override;

private:
    GlimmerIrradianceShaderData m_shaderData;
};

} // namespace Hyperion
