/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/DepthOfField.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/Frame.hpp>

namespace hyperion {

DOFBlur::DOFBlur(const Vec2u& extent, GBuffer* gbuffer)
    : m_gbuffer(gbuffer),
      m_extent(extent)
{
}

DOFBlur::~DOFBlur() = default;

void DOFBlur::Create()
{
    ShaderRef blurHorizontalShader = g_shaderManager->GetOrCreate(NAME("DOFBlurDirection"), ShaderProperties({ ShaderProperty(NAME("DIRECTION"), NAME("HORIZONTAL")) }));
    Assert(blurHorizontalShader.IsValid());

    m_blurHorizontalPass = CreateObject<FullScreenPass>(blurHorizontalShader, TF_RGBA8, m_extent, m_gbuffer);
    m_blurHorizontalPass->Create();

    ShaderRef blurVerticalShader = g_shaderManager->GetOrCreate(NAME("DOFBlurDirection"), ShaderProperties({ ShaderProperty(NAME("DIRECTION"), NAME("VERTICAL")) }));
    Assert(blurVerticalShader.IsValid());

    m_blurVerticalPass = CreateObject<FullScreenPass>(blurVerticalShader, TF_RGBA8, m_extent, m_gbuffer);
    m_blurVerticalPass->Create();

    ShaderRef blurMixShader = g_shaderManager->GetOrCreate(NAME("DOFBlurMix"));
    Assert(blurMixShader.IsValid());

    m_blurMixPass = CreateObject<FullScreenPass>(blurMixShader, TF_RGBA8, m_extent, m_gbuffer);
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
        pass->SetPushConstants(&pushConstants, sizeof(pushConstants));
        pass->Render(frame, renderSetup);
    }

    m_blurMixPass->SetPushConstants(&pushConstants, sizeof(pushConstants));
    m_blurMixPass->Render(frame, renderSetup);
}

} // namespace hyperion
