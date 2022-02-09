#include "skydome_shader.h"
#include "../environment.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"
#include "../../math/math_util.h"

namespace hyperion {
SkydomeShader::SkydomeShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("shaders/skydome.vert");
    const std::string fs_path("shaders/skydome.frag");

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

    if (properties.HasValue("CLOUDS")) {
        has_clouds = properties.GetValue("CLOUDS").IsTruthy();
    } else {
        has_clouds = false;
    }

    if (has_clouds) {
        noise_map = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/clouds.png");
        if (noise_map == nullptr) {
            throw std::runtime_error("Could not load noise map!");
        }
    }
    m_global_time = 0.0f;

    sun_color = Vector4(0.05f, 0.02f, 0.01f, 1.0f);

    wavelength = Vector3(0.731f, 0.612f, 0.455f);
    inv_wavelength4.x = 1.0f / pow(wavelength.x, 4.0f);
    inv_wavelength4.y = 1.0f / pow(wavelength.y, 4.0f);
    inv_wavelength4.z = 1.0f / pow(wavelength.z, 4.0f);

    num_samples = 4;
    Kr = 0.0025f;
    Km = 0.0015f;
    ESun = 100.0f;
    KrESun = Kr * ESun;
    KmESun = Km * ESun;
    Kr4PI = Kr * 4.0f * MathUtil::PI;
    Km4PI = Km * 4.0f * MathUtil::PI;
    exposure = 2.0f;
    G = -0.990f;
    inner_radius = 100.0f;
    scale = 1.0f / (inner_radius * 1.025f - inner_radius);
    scale_depth = 0.25f;
    scale_over_scale_depth = scale / scale_depth;

    // Set uniform constants
    SetUniform("fKrESun", KrESun);
    SetUniform("fKmESun", KmESun);
    SetUniform("fOuterRadius", inner_radius * 1.025f);
    SetUniform("fInnerRadius", inner_radius);
    SetUniform("fOuterRadius2", (float)pow(inner_radius * 1.025f, 2.0f));
    SetUniform("fInnerRadius2", inner_radius * inner_radius);
    SetUniform("fKr4PI", Kr4PI);
    SetUniform("fKm4PI", Km4PI);
    SetUniform("fg", G);
    SetUniform("fg2", G * G);
    SetUniform("fExposure", exposure);
}

void SkydomeShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);

    auto *env = Environment::GetInstance();

    if (has_clouds) {
        noise_map->Prepare();

        SetUniform("u_noiseMap", noise_map.get());
    }

    SetUniform("u_globalTime", m_global_time);
    SetUniform("v3LightPos", env->GetSun().GetDirection());
    SetUniform("u_skyColor", env->GetSun().GetColor());
    SetUniform("u_sunColor", sun_color);
}

void SkydomeShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    // Sky dome should follow the camera
    Transform updated_transform(transform);
    updated_transform.SetTranslation(Vector3(
        camera->GetTranslation().x,
        camera->GetTranslation().y - 5.0,
        camera->GetTranslation().z
    ));

    Shader::ApplyTransforms(updated_transform, camera);

    SetUniform("v3CameraPos", camera->GetTranslation());
}

void SkydomeShader::SetGlobalTime(float global_time)
{
    m_global_time = global_time;
}
} // namespace hyperion
