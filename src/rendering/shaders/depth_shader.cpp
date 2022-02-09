#include "depth_shader.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"

namespace hyperion {
DepthShader::DepthShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("shaders/default.vert");
    const std::string fs_path("shaders/depth.frag");

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

void DepthShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);

    if (auto diffuse = mat.GetTexture("DiffuseMap")) {
        diffuse->Prepare();

        SetUniform("DiffuseMap", diffuse.get());
        SetUniform("HasDiffuseMap", 1);
    }
}

void DepthShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
    SetUniform("u_camerapos", camera->GetTranslation());
}
} // namespace hyperion
