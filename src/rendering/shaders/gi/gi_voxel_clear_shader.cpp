#include "gi_voxel_clear_shader.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"

namespace hyperion {
GIVoxelClearShader::GIVoxelClearShader(const ShaderProperties &properties)
    : ComputeShader(properties)
{
    const std::string cs_path("shaders/gi/clear.comp");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_COMPUTE,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(cs_path)->GetText(),
        properties,
        cs_path
    );
}
} // namespace hyperion
