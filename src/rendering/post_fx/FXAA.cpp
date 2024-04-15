/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include "FXAA.hpp"
#include <rendering/Shader.hpp>
#include <rendering/PostFX.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

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

Handle<Shader> FXAAEffect::CreateShader()
{
    return g_shader_manager->GetOrCreate(HYP_NAME(FXAA));
}

} // namespace hyperion::v2