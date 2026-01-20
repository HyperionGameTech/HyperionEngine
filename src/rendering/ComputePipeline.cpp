/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/ComputePipeline.hpp>
#include <rendering/Shader.hpp>

#include <ComputePipeline.generated.inl>

namespace Hyperion {

#pragma region ComputePipelineBase

bool ComputePipelineBase::MatchesSignature(const ShaderDefinition& shaderDefinition) const
{
    if (!m_shader.IsValid())
    {
        return false;
    }

    const ShaderDefinition& currentShaderDefinition = m_shader->GetCompiledShader()->GetDefinition();

    if (shaderDefinition.name != currentShaderDefinition.name)
    {
        return false;
    }

    if (shaderDefinition.GetProperties().GetPropertySetHashCode() != currentShaderDefinition.properties.GetPropertySetHashCode())
    {
        return false;
    }

    return true;
}

#pragma endregion ComputePipelineBase

} // namespace Hyperion
