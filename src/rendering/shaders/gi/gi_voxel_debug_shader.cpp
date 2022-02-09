#include "gi_voxel_debug_shader.h"
#include "../../environment.h"
#include "../../shader_manager.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../../util/shader_preprocessor.h"

namespace hyperion {
GIVoxelDebugShader::GIVoxelDebugShader(const ShaderProperties &properties)
    : LightingShader(properties)
{
    const std::string vs_path("shaders/gi/voxel.vert");
    const std::string fs_path("shaders/gi/gi_debug.frag");
    const std::string gs_path("shaders/gi/voxel.geom");

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

    if (ShaderManager::GetInstance()->GetBaseShaderProperties().GetValue("VCT_GEOMETRY_SHADER").IsTruthy()) {
        AddSubShader(
            Shader::SubShaderType::SUBSHADER_GEOMETRY,
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(gs_path)->GetText(),
            properties,
            gs_path
        );
    }
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