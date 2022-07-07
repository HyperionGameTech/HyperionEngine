#include "SSAO.hpp"
#include <rendering/Shader.hpp>
#include <rendering/PostFX.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

SSAOEffect::SSAOEffect()
    : PostProcessingEffect(
          stage,
          index,
          Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8
      )
{
}

SSAOEffect::~SSAOEffect() = default;

Ref<Shader> SSAOEffect::CreateShader(Engine *engine)
{
    return engine->resources.shaders.Add(std::make_unique<Shader>(
        std::vector<SubShader>{
            SubShader{ShaderModule::Type::VERTEX, {
                Reader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/filter_pass_vert.spv")).ReadBytes()
            }},
            SubShader{ShaderModule::Type::FRAGMENT, {
                Reader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/filter_pass_frag.spv")).ReadBytes()
            }}
        }
    ));
}

} // namespace hyperion::v2