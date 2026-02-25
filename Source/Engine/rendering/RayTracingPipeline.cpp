/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RayTracingPipeline.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/Shader.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <RayTracingPipeline.generated.inl>

namespace Hyperion {

#pragma region RayTracingPipelineBase

bool RayTracingPipelineBase::MatchesSignature(const ShaderDesc& shaderDesc) const
{
    if (!m_shaderInstance.IsValid())
    {
        return false;
    }

    const Shader& shader = *m_shaderInstance->GetShader();

    if (shaderDesc.name != shader.baseName || shaderDesc.properties != shader.properties)
    {
        return false;
    }

    return true;
}

#pragma endregion RayTracingPipelineBase

} // namespace Hyperion
