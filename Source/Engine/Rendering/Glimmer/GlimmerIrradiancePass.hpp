/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/FullScreenPass.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class GBuffer;
class GlimmerBrickReflections;

struct GlimmerIrradianceShaderData
{
    Vec4u params; // x = 1 when the target holds Glimmer this frame (2 when it holds the reference's irradiance in place of it), y = 1 when it holds a debug view in place of the irradiance, z = 1 when sky reflections use its specular visibility, w = 1 when the bricks have reflections
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

    const GpuImageViewRef& GetSpecularImageView() const;
    const GpuImageViewRef& GetReflectionImageView() const;
    const GpuImageViewRef& GetBrickReflectionImageView() const;

    virtual void CreateFramebuffer() override;

    virtual void Render(Frame* frame, const RenderSetup& renderSetup) override;

private:
    FramebufferRef m_halfFramebuffer;
    UniquePtr<GlimmerBrickReflections> m_brickReflections;
    GlimmerIrradianceShaderData m_shaderData;
    uint32 m_frameIndex;
};

} // namespace Hyperion
