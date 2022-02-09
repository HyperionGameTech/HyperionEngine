#include "sh_compute_shader.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"

namespace hyperion {
SHComputeShader::SHComputeShader(const ShaderProperties &properties)
    : ComputeShader(properties)
{
    const std::string cs_path("shaders/compute/sh.comp");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_COMPUTE,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(cs_path)->GetText(),
        properties,
        cs_path
    );
}

SHComputeShader::~SHComputeShader()
{
}
} // namespace hyperion