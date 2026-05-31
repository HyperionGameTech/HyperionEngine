/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/RayTracingPipeline.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/Shader.hpp>

#include <Rendering/util/ShaderCompiler.hpp>

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

    if (shaderDesc.name != shader.baseName || ((shader.properties & shaderDesc.properties) != shader.properties))
    {
        return false;
    }

    return true;
}

#pragma endregion RayTracingPipelineBase

} // namespace Hyperion
