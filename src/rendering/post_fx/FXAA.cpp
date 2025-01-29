/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/post_fx/FXAA.hpp>
#include <rendering/Shader.hpp>
#include <rendering/PostFX.hpp>

namespace hyperion {

FXAAEffect::FXAAEffect()
    : PostProcessingEffect(stage, index)
{
}

FXAAEffect::~FXAAEffect() = default;

void FXAAEffect::OnAdded()
{
}

void FXAAEffect::OnRemoved()
{
}

ShaderRef FXAAEffect::CreateShader()
{
    return ShaderManager::GetInstance()->GetOrCreate(NAME("FXAA"));
}

} // namespace hyperion