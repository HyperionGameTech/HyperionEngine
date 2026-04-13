/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/ComputePipeline.hpp>
#include <rendering/ShaderInstance.hpp>
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

    if (shaderDesc.name != shader.baseName || ((shader.properties & shaderDesc.properties) != shader.properties))
    {
        return false;
    }

    return true;
}

#pragma endregion ComputePipelineBase

} // namespace Hyperion
