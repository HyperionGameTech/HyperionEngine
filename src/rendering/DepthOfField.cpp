/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/DepthOfField.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/Frame.hpp>

namespace Hyperion {

DOFBlur::DOFBlur(const Vec2u& extent, GBuffer* gbuffer)
    : m_gbuffer(gbuffer),
      m_extent(extent)
{
}

DOFBlur::~DOFBlur() = default;

void DOFBlur::Create()
{
    m_blurHorizontalPass = MakeHandle<FullScreenPass>(
        ShaderDefinition(NAME("DOFBlurDirection"), ShaderProperties({ ShaderProperty(NAME("DIRECTION"), NAME("HORIZONTAL")) })),
        TF_RGBA8,
        m_extent,
        m_gbuffer);

    m_blurHorizontalPass->Create();

    m_blurVerticalPass = MakeHandle<FullScreenPass>(
        ShaderDefinition(NAME("DOFBlurDirection"), ShaderProperties({ ShaderProperty(NAME("DIRECTION"), NAME("VERTICAL")) })),
        TF_RGBA8,
        m_extent,
        m_gbuffer);

    m_blurVerticalPass->Create();

    m_blurMixPass = MakeHandle<FullScreenPass>(
        ShaderDefinition(NAME("DOFBlurMix")),
        TF_RGBA8,
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
    struct
    {
        Vec2u dimension;
    } pushConstants;

    pushConstants.dimension = m_extent;

    const uint32 frameIndex = frame->GetFrameIndex();

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
