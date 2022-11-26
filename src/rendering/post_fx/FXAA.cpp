#include "FXAA.hpp"
#include <rendering/Shader.hpp>
#include <rendering/PostFX.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

FXAAEffect::FXAAEffect()
    : PostProcessingEffect(stage, index)
{
}

FXAAEffect::~FXAAEffect() = default;

Handle<Shader> FXAAEffect::CreateShader()
{
    return Engine::Get()->CreateObject<Shader>(
        std::vector<SubShader> {
            SubShader{ShaderModule::Type::VERTEX, {
                Reader(FileSystem::Join(Engine::Get()->GetAssetManager().GetBasePath().Data(), "/vkshaders/PostEffect.vert.spv")).ReadBytes()
            }},
            SubShader{ShaderModule::Type::FRAGMENT, {
                Reader(FileSystem::Join(Engine::Get()->GetAssetManager().GetBasePath().Data(), "/vkshaders/fxaa.frag.spv")).ReadBytes()
            }}
        }
    );
}

} // namespace hyperion::v2