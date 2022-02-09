#include "fur_shader.h"
#include "../environment.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"

namespace hyperion {
FurShader::FurShader(const ShaderProperties &properties)
    : LightingShader(properties)
{
    const std::string vs_path("shaders/fur.vert");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_VERTEX,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
        properties,
        vs_path
    );

    const std::string fs_path("shaders/fur.frag");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_FRAGMENT,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
        properties,
        fs_path
    );

    const std::string gs_path("shaders/fur.geom");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_GEOMETRY,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(gs_path)->GetText(),
        properties,
        gs_path
    );
}

void FurShader::ApplyMaterial(const Material &mat)
{
    LightingShader::ApplyMaterial(mat);

}

void FurShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    LightingShader::ApplyTransforms(transform, camera);
}
} // namespace hyperion
