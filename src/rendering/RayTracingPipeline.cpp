/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RayTracingPipeline.hpp>
#include <rendering/Shader.hpp>

#include <RayTracingPipeline.generated.inl>

namespace Hyperion {

#pragma region RayTracingPipelineBase

bool RayTracingPipelineBase::MatchesSignature(const ShaderDefinition& shaderDefinition) const
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

#pragma endregion RayTracingPipelineBase

} // namespace Hyperion
