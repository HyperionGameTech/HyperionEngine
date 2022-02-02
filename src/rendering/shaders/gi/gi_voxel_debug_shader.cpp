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
    const std::string gs_path("res/shaders/gi/voxel.geom");
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

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_GEOMETRY,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(gs_path)->GetText(),
        properties,
        gs_path
    );


    for (int i = 0; i < 16; i++) {
        SetUniform("poissonDisk[" + std::to_string(i) + "]",
            Environment::possion_disk[i]);
    }
}

void GIVoxelDebugShader::ApplyMaterial(const Material &mat)
{
    LightingShader::ApplyMaterial(mat);

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

    if (auto cubemap = env->GetGlobalCubemap()) {
        //cubemap->Prepare();

        //SetUniform("env_GlobalCubemap", cubemap.get());


        /*if (env->ProbeEnabled()) {
            const auto &origin = env->GetProbeRenderer()->GetProbe()->GetOrigin();
            SetUniform("EnvProbe.position", origin);
            SetUniform("EnvProbe.max", Vector3(40.0f));
        }*/
    }

    if (auto cubemap = env->GetGlobalIrradianceCubemap()) {
        cubemap->Prepare();

        SetUniform("env_GlobalIrradianceCubemap", cubemap.get());
    }

    for (auto it = mat.textures.begin(); it != mat.textures.end(); it++) {
        if (it->second == nullptr) {
            continue;
        }

        it->second->Prepare();

        SetUniform(it->first, it->second.get());
        SetUniform(std::string("Has") + it->first, 1);
    }

    if (mat.HasParameter("shininess")) {
        SetUniform("u_shininess", mat.GetParameter("shininess")[0]);
    }

    if (mat.HasParameter("roughness")) {
        SetUniform("u_roughness", mat.GetParameter("roughness")[0]);
    }

    if (mat.HasParameter("RimShading")) {
        SetUniform("RimShading", mat.GetParameter("RimShading")[0]);
    }

    SetUniform("u_scale", 5.0f);
    SetUniform("u_probePos", Vector3(0.0));//Environment::GetInstance()->GetGIRenderer()->GetGIMapping(0)->GetProbePosition());

    Matrix4 tmp;

    BoundingBox bb = Environment::GetInstance()->GetGIManager()->GetProbe(0)->GetAABB();
    Matrix4 projection;
    MatrixUtil::ToOrtho(projection, bb.GetMin().x, bb.GetMax().x, bb.GetMin().y, bb.GetMax().y, 0.0f, bb.GetMax().z - bb.GetMin().z);
    MatrixUtil::ToLookAt(tmp, Vector3(bb.GetMax().x, 0, 0) + bb.GetCenter(),
        bb.GetCenter(), Vector3(0, 1, 0));
    Matrix4 mvp_x = projection * tmp;
    MatrixUtil::ToLookAt(tmp, Vector3(0, bb.GetMax().y, 0) + bb.GetCenter(),
        bb.GetCenter(), Vector3(0, 0, -1));
    Matrix4 mvp_y = projection * tmp;
    MatrixUtil::ToLookAt(tmp, Vector3(0, 0, bb.GetMax().z) + bb.GetCenter(),
        bb.GetCenter(), Vector3(0, 1, 0));
    Matrix4 mvp_z = projection * tmp;

    SetUniform("mvp_x", mvp_x);
    SetUniform("mvp_y", mvp_y);
    SetUniform("mvp_z", mvp_z);
}

void GIVoxelDebugShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    LightingShader::ApplyTransforms(transform, camera);


    Vector3 voxel_bias(20.0f); // todo

    Transform world_to_voxel_tex(Vector3(voxel_bias), Vector3(5.0f)/* scale*/, Quaternion());
    SetUniform("WorldToVoxelTexMatrix", world_to_voxel_tex.GetMatrix());
}
} // namespace hyperion
