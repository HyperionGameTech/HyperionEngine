/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/FullScreenPass.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Rendering/Sampler.hpp>

namespace Hyperion {

class Texture;
class GBuffer;

class BloomPass final : public FullScreenPass
{
public:
    BloomPass(Vec2u extent, GBuffer* gbuffer);
    BloomPass(const BloomPass& other) = delete;
    BloomPass& operator=(const BloomPass& other) = delete;
    ~BloomPass() override;

    const Handle<Texture>& GetBloomResult() const;

    void Create() override;

    void Render(Frame* frame, const RenderSetup& renderSetup) override;

protected:
    bool UsesTemporalBlending() const override
    {
        return false;
    }

    bool ShouldRenderHalfRes() const override
    {
        return false;
    }

    void Resize_Internal(Vec2u newSize) override;

    ShaderPropertySet GetShaderProperties() const;

private:
    void ExtractBrightAreas(Frame* frame, const RenderSetup& renderSetup, const FramebufferRef& inputsFramebuffer, class DeferredPassData* dpd);
    void Downsample(Frame* frame, const RenderSetup& renderSetup);
    void Upsample(Frame* frame, const RenderSetup& renderSetup);

    static constexpr uint32 NumMipLevels = 6;

    Handle<Texture> m_brightExtractTexture;
    FixedArray<UniquePtr<FullScreenPass>, NumMipLevels - 1> m_downsamplePasses;
    FixedArray<UniquePtr<FullScreenPass>, NumMipLevels - 1>  m_upsamplePasses;

    Handle<Texture> m_bloomResult;
    Sampler* m_samplerClampToEdge;
};

} // namespace Hyperion
