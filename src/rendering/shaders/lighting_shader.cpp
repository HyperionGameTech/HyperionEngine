#include "lighting_shader.h"
#include "../environment.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"

namespace hyperion {
LightingShader::LightingShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("shaders/default.vert");
    const std::string fs_path("shaders/default.frag");

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

    for (int i = 0; i < 16; i++) {
        SetUniform("poissonDisk[" + std::to_string(i) + "]",
            Environment::possion_disk[i]);
    }
}

void LightingShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);

    SetUniform("u_diffuseColor", mat.diffuse_color);

    auto *env = Environment::GetInstance();
    if (env->ShadowsEnabled()) {
        for (int i = 0; i < env->NumCascades(); i++) {
            const std::string i_str = std::to_string(i);

            if (auto shadow_map = env->GetShadowMap(i)) {
                shadow_map->Prepare();

                SetUniform("u_shadowMap[" + i_str + "]", shadow_map.get());
            }

            SetUniform("u_shadowMatrix[" + i_str + "]", env->GetShadowMatrix(i));
            SetUniform("u_shadowSplit[" + i_str + "]", (float)env->GetShadowSplit(i));
        }
    }

    // if (!m_properties.GetValue("DEFERRED").IsTruthy()) {

    env->GetSun().Bind(0, this);

    SetUniform("env_NumPointLights", (int)env->GetNumPointLights());

    for (int i = 0; i < env->GetNumPointLights(); i++) {
        if (auto point_light = env->GetPointLight(i)) {
            point_light->Bind(i, this);
        }
    }

    /*if (auto gi = Environment::GetInstance()->GetGIRenderer()->GetGIMapping(0)->GetShadowMap()) {
        gi->Prepare();

        SetUniform("env_GlobalCubemap", gi.get());
    }*/


    for (int i = 0; i < env->GetProbeManager()->NumProbes(); i++) {
        env->GetProbeManager()->GetProbe(i)->Bind(this);
    }

    if (!env->GetProbeManager()->EnvMapEnabled()) {
        if (auto cubemap = env->GetGlobalCubemap()) {
            cubemap->Prepare();

            SetUniform("env_GlobalCubemap", cubemap.get());
        }

        if (auto cubemap = env->GetGlobalIrradianceCubemap()) {
            cubemap->Prepare();

            SetUniform("env_GlobalIrradianceCubemap", cubemap.get());
        }
    }

    for (auto it = mat.textures.begin(); it != mat.textures.end(); it++) {
        if (it->second == nullptr) {
            continue;
        }

        it->second->Prepare();

        SetUniform(it->first, it->second.get());
        SetUniform(std::string("Has") + it->first, 1);
    }

    SetUniform("u_shininess", mat.GetParameter(MATERIAL_PARAMETER_METALNESS)[0]);
    SetUniform("u_roughness", mat.GetParameter(MATERIAL_PARAMETER_ROUGHNESS)[0]);


}

void LightingShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
    SetUniform("u_camerapos", camera->GetTranslation());
}
} // namespace hyperion
