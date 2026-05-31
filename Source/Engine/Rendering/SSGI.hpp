/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Rendering/TemporalBlending.hpp>
#include <Rendering/FullScreenPass.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Core/config/Config.hpp>

#include <Core/reflection/ObjectMacros.hpp>

#include <Core/utilities/EnumFlags.hpp>

namespace Hyperion {

class GBuffer;
class View;

class SSGI : public FullScreenPass
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    explicit SSGI(GBuffer* gbuffer);

    ~SSGI() override;

    const Handle<Texture>& GetFinalResultTexture() const;

    void Create() override;
    void Render(Frame* frame, const RenderSetup& renderSetup) override;

private:
    static constexpr uint32 NumDownsamplePasses = 4;

    Handle<Texture> m_ssgiTexture;

    Handle<Texture> m_downsampleTextures[NumDownsamplePasses];
    UniquePtr<FullScreenPass> m_upsamplePasses[NumDownsamplePasses];

    UniquePtr<TemporalBlending> m_temporalBlending;
};

} // namespace Hyperion
