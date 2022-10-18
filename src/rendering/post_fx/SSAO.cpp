#include "SSAO.hpp"
#include <rendering/Shader.hpp>
#include <rendering/PostFX.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

SSAOEffect::SSAOEffect()
    : PostProcessingEffect(
          stage,
          index,
          Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F
      )
{
}

SSAOEffect::~SSAOEffect() = default;

Handle<Shader> SSAOEffect::CreateShader(Engine *engine)
{
    return engine->CreateHandle<Shader>(
        std::vector<SubShader>{
            SubShader{ShaderModule::Type::VERTEX, {
                Reader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "/vkshaders/PostEffect.vert.spv")).ReadBytes()
            }},
            SubShader{ShaderModule::Type::FRAGMENT, {
                Reader(FileSystem::Join(engine->GetAssetManager().GetBasePath().Data(), "/vkshaders/SSAO.frag.spv")).ReadBytes()
            }}
        }
    );
}

} // namespace hyperion::v2