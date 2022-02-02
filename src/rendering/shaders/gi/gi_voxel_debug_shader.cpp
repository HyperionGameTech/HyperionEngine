#include "gi_voxel_debug_shader.h"
#include "../../environment.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../../util/shader_preprocessor.h"

namespace hyperion {
GIVoxelDebugShader::GIVoxelDebugShader(const ShaderProperties &properties)
    : LightingShader(properties)
{
    const std::string vs_path("res/shaders/gi/voxel.vert");
    const std::string fs_path("res/shaders/gi/gi_debug.frag");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_VERTEX,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
        properties,
        vs_path
    );

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_FRAGMENT,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
        properties,
        fs_path
    );
}

void GIVoxelDebugShader::ApplyMaterial(const Material &mat)
{
    LightingShader::ApplyMaterial(mat);
}

void GIVoxelDebugShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    LightingShader::ApplyTransforms(transform, camera);
}
} // namespace hyperion
