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

    const CompiledShader& compiledShader = *m_shader->GetCompiledShader();

    if (shaderDefinition.name != compiledShader.name)
    {
        return false;
    }

    if (shaderDefinition.GetProperties().GetPropertySetHashCode() != compiledShader.propertySetHashCode)
    {
        return false;
    }

    return true;
}

#pragma endregion RayTracingPipelineBase

} // namespace Hyperion
