/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/PostFx/FXAA.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/PostFX.hpp>

namespace Hyperion {

FXAAEffect::FXAAEffect(GBuffer* gbuffer)
    : PostProcessingEffect(stage, index, TextureFormat::RGBA8, gbuffer)
{
}

FXAAEffect::~FXAAEffect() = default;

void FXAAEffect::OnAdded()
{
}

void FXAAEffect::OnRemoved()
{
}

ShaderDesc FXAAEffect::GetShaderDesc()
{
    return ShaderDesc(NAME("FXAA"));
}

} // namespace Hyperion
