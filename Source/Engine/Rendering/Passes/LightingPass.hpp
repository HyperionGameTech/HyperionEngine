/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/FullScreenPass.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Scene/Light.hpp> // For LightType

namespace Hyperion {

class GBuffer;
class Texture;
class DeferredPassData;
class RenderProxyList;

enum DeferredPassMode : uint32
{
    DPM_INDIRECT_LIGHTING,
    DPM_DIRECT_LIGHTING
};

class LightingPass final : public FullScreenPass
{
public:
    LightingPass(DeferredPassMode mode, Vec2u extent, GBuffer* gbuffer, const FramebufferRef& framebuffer);
    LightingPass(const LightingPass& other) = delete;
    LightingPass& operator=(const LightingPass& other) = delete;
    virtual ~LightingPass() override;

    virtual void Create() override;

protected:
    virtual void RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& rs, Framebuffer* framebuffer) override;

    virtual void Resize_Internal(Vec2u newSize) override;

private:
    const DeferredPassMode m_mode;

    Handle<Texture> m_ltcMatrixTexture;
    Handle<Texture> m_ltcBrdfTexture;
    Sampler* m_ltcSampler;
};

namespace DeferredRendererHelpers {

void GetDeferredShaderProperties(
    DeferredPassMode mode,
    ShaderPropertySet& outShaderProperties,
    const RenderProxyList* rpl = nullptr,
    LightType lightType = InvalidLightType,
    bool clustered = false);

} // namespace DeferredRendererHelpers

} // namespace Hyperion
