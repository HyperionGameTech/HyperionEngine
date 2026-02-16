/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/ComputePipeline.hpp>
#include <rendering/Shader.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <ComputePipeline.generated.inl>

namespace Hyperion {

#pragma region ComputePipelineBase

bool ComputePipelineBase::MatchesSignature(const ShaderDesc& shaderDesc) const
{
    if (!m_shaderInstance.IsValid())
    {
        return false;
    }

    const Shader& shader = *m_shaderInstance->GetShader();

    if (shaderDesc.name != shader.name || shaderDesc.properties != shader.properties)
    {
        return false;
    }

    return true;
}

#pragma endregion ComputePipelineBase

} // namespace Hyperion
