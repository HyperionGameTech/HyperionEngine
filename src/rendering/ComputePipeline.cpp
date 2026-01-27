/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/ComputePipeline.hpp>
#include <rendering/Shader.hpp>

#include <ComputePipeline.generated.inl>

namespace Hyperion {

#pragma region ComputePipelineBase

bool ComputePipelineBase::MatchesSignature(const ShaderDesc& shaderDesc) const
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

#pragma endregion ComputePipelineBase

} // namespace Hyperion
