#include "clouds_shader.h"
#include "../environment.h"
#include "../../core_engine.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"

namespace hyperion {
CloudsShader::CloudsShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("shaders/clouds.vert");
    const std::string fs_path("shaders/clouds.frag");

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

    cloud_map = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/clouds2.png");
    if (cloud_map == nullptr) {
        throw std::runtime_error("Could not load cloud map!");
    }

    m_global_time = 0.0f;
    m_cloud_color = Vector4(1.0);
}

void CloudsShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);

    if (cloud_map != nullptr) {
        cloud_map->Prepare();
        SetUniform("m_CloudMap", cloud_map.get());
    }

    SetUniform("m_GlobalTime", m_global_time);
    SetUniform("m_CloudColor", m_cloud_color);
}

void CloudsShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
     // Cloud layer should follow the camera
    Transform updated_transform(transform);
    updated_transform.SetTranslation(Vector3(
        camera->GetTranslation().x,
        camera->GetTranslation().y + 50.0f,
        camera->GetTranslation().z
    ));

    Shader::ApplyTransforms(updated_transform, camera);
}

void CloudsShader::SetCloudColor(const Vector4 &cloud_color)
{
    m_cloud_color = cloud_color;
}

void CloudsShader::SetGlobalTime(float global_time)
{
    m_global_time = global_time;
}
} // namespace hyperion
