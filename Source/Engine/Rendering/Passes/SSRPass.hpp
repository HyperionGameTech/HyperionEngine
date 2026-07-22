/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Rendering/TemporalBlending.hpp>
#include <Rendering/FullScreenPass.hpp>

#include <Rendering/RenderTypes.hpp>

#include <Core/Config/Config.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Utilities/EnumFlags.hpp>

namespace Hyperion {

class GBuffer;

class SSRPass final : public FullScreenPass
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    explicit SSRPass(Vec2u extent, GBuffer* gbuffer);
    ~SSRPass();

    HYP_FORCE_INLINE Texture* GetUVsTexture() const
    {
        return m_uvsTexture;
    }

    HYP_FORCE_INLINE Texture* GetSampledResultTexture() const
    {
        return m_sampledResultTexture;
    }

    Texture* GetFinalResultTexture() const;

    HYP_FORCE_INLINE bool IsRendered() const
    {
        return m_isRendered;
    }

    void Create() override;
    void Render(Frame* frame, const RenderSetup& renderSetup) override;

private:
    ShaderPropertySet GetShaderProperties() const;

    void CreatePasses();

    void UpdatePipelineState(Frame* frame, const RenderSetup& renderSetup);

    Handle<Texture> m_uvsTexture;
    Handle<Texture> m_sampledResultTexture;

    FullScreenPass* m_tracePass;
    FullScreenPass* m_samplePass;

    UniquePtr<TemporalBlending> m_temporalBlending;

    DelegateHandler m_onGbufferResolutionChanged;

    bool m_isRendered;
};

} // namespace Hyperion
