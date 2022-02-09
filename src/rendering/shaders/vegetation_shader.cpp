#include "vegetation_shader.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"

namespace hyperion {
VegetationShader::VegetationShader(const ShaderProperties &properties)
    : LightingShader(
        ShaderProperties()
            .Define("VEGETATION_FADE", true)
            .Define("VEGETATION_LIGHTING", true)
            .Merge(properties)
    )
{
    const std::string vs_path("shaders/vegetation.vert");
    const std::string fs_path("shaders/vegetation.frag");

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

void VegetationShader::ApplyMaterial(const Material &mat)
{
    LightingShader::ApplyMaterial(mat);
}

void VegetationShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    LightingShader::ApplyTransforms(transform, camera); // TODO: moving in wind
}
} // namespace hyperion
