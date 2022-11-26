#include "SSAO.hpp"
#include <rendering/Shader.hpp>
#include <rendering/PostFX.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

SSAOEffect::SSAOEffect()
    : PostProcessingEffect(
          stage,
          index,
          InternalFormat::RGBA16F
      )
{
}

SSAOEffect::~SSAOEffect() = default;

Handle<Shader> SSAOEffect::CreateShader()
{
    return Engine::Get()->CreateObject<Shader>(
        std::vector<SubShader>{
            SubShader{ShaderModule::Type::VERTEX, {
                Reader(FileSystem::Join(Engine::Get()->GetAssetManager().GetBasePath().Data(), "/vkshaders/PostEffect.vert.spv")).ReadBytes()
            }},
            SubShader{ShaderModule::Type::FRAGMENT, {
                Reader(FileSystem::Join(Engine::Get()->GetAssetManager().GetBasePath().Data(), "/vkshaders/SSAO.frag.spv")).ReadBytes()
            }}
        }
    );
}

} // namespace hyperion::v2