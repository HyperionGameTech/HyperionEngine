/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/DepthOfField.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/ShaderPropertyDictionary.hpp>

namespace Hyperion {

DOFBlur::DOFBlur(const Vec2u& extent, GBuffer* gbuffer)
    : m_gbuffer(gbuffer),
      m_extent(extent)
{
}

DOFBlur::~DOFBlur() = default;

void DOFBlur::Create()
{
    m_blurHorizontalPass = MakeUnique<FullScreenPass>(
        ShaderDesc(NAME("DOFBlurDirection"), ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("DIRECTION"), NAME("HORIZONTAL"))) } }),
        TextureFormat::RGBA8,
        m_extent,
        m_gbuffer);

    m_blurHorizontalPass->Create();

    m_blurVerticalPass = MakeUnique<FullScreenPass>(
        ShaderDesc(NAME("DOFBlurDirection"), ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("DIRECTION"), NAME("VERTICAL"))) } }),
        TextureFormat::RGBA8,
        m_extent,
        m_gbuffer);

    m_blurVerticalPass->Create();

    m_blurMixPass = MakeUnique<FullScreenPass>(
        ShaderDesc(NAME("DOFBlurMix")),
        TextureFormat::RGBA8,
        m_extent,
        m_gbuffer);

    m_blurMixPass->Create();
}

void DOFBlur::Destroy()
{
    m_blurHorizontalPass.Reset();
    m_blurVerticalPass.Reset();
    m_blurMixPass.Reset();
}

void DOFBlur::Render(Frame* frame, const RenderSetup& renderSetup)
{
    FixedArray<FullScreenPass*, 2> directionalPasses {
        m_blurHorizontalPass.Get(),
        m_blurVerticalPass.Get()
    };

    for (FullScreenPass* pass : directionalPasses)
    {
        pass->Render(frame, renderSetup);
    }

    m_blurMixPass->Render(frame, renderSetup);
}

} // namespace Hyperion
