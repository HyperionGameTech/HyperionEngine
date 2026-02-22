/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/post_fx/FXAA.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/PostFX.hpp>

namespace Hyperion {

FXAAEffect::FXAAEffect(GBuffer* gbuffer)
    : PostProcessingEffect(stage, index, TextureFormat::RGBA8, gbuffer)
{
}

FXAAEffect::~FXAAEffect() = default;

void FXAAEffect::OnAdded()
{
}

void FXAAEffect::OnRemoved()
{
}

ShaderDesc FXAAEffect::GetShaderDesc()
{
    return ShaderDesc(NAME("FXAA"));
}

} // namespace Hyperion
