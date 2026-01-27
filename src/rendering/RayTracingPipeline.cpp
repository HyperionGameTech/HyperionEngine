/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RayTracingPipeline.hpp>
#include <rendering/Shader.hpp>

#include <RayTracingPipeline.generated.inl>

namespace Hyperion {

#pragma region RayTracingPipelineBase

bool RayTracingPipelineBase::MatchesSignature(const ShaderDesc& shaderDesc) const
{
    if (!m_shader.IsValid())
    {
        return false;
    }

    const CompiledShader& compiledShader = *m_shader->GetCompiledShader();

    if (shaderDesc.name != compiledShader.name || shaderDesc.properties != compiledShader.properties)
    {
        return false;
    }

    return true;
}

#pragma endregion RayTracingPipelineBase

} // namespace Hyperion
