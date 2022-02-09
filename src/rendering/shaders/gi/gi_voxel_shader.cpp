#include "gi_voxel_shader.h"
#include "../../shader_manager.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../environment.h"

#include "../../../core_engine.h"

namespace hyperion {
GIVoxelShader::GIVoxelShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("shaders/gi/voxel.vert");
    const std::string fs_path("shaders/gi/voxel.frag");
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

void GIVoxelShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);

    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::CULL_FACE);

    SetUniform("C_albedo", mat.diffuse_color);
    SetUniform("Emissiveness", mat.GetParameter(MATERIAL_PARAMETER_EMISSIVENESS)[0]);

    for (auto it = mat.textures.begin(); it != mat.textures.end(); it++) {
        if (it->second == nullptr) {
            continue;
        }

        it->second->Prepare();

        SetUniform(it->first, it->second.get());
        SetUniform(std::string("Has") + it->first, 1);
    }
}

void GIVoxelShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
    SetUniform("CameraPosition", camera->GetTranslation());

    // TODO filter by type
    for (int i = 0; i < Environment::GetInstance()->GetProbeManager()->NumProbes(); i++) {
        if (auto &probe = Environment::GetInstance()->GetProbeManager()->GetProbe(i)) {
            probe->Bind(this);
        }
    }
}
} // namespace hyperion
