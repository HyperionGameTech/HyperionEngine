#include "ui_object_shader.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../../util/shader_preprocessor.h"

namespace hyperion {
UIObjectShader::UIObjectShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("shaders/ui/ui_object.vert");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_VERTEX,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
        properties,
        vs_path
    );

    const std::string fs_path("shaders/ui/ui_object.frag");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_FRAGMENT,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
        properties,
        fs_path
    );
}

void UIObjectShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);

    for (auto it = mat.textures.begin(); it != mat.textures.end(); it++) {
        if (it->second == nullptr) {
            continue;
        }

        it->second->Prepare();

        SetUniform(it->first, it->second.get());
        SetUniform(std::string("Has") + it->first, 1);
    }
}

void UIObjectShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);

    Transform model_2d;
    
    Vector2 screen_scale(1.0f / camera->GetWidth(), 1.0f / camera->GetHeight());
    model_2d.SetScale(Vector3(
        screen_scale.x * transform.GetScale().x,
        screen_scale.y * transform.GetScale().y,
        1.0
    ));

    model_2d.SetTranslation(transform.GetTranslation());

    // model_2d.SetTranslation(Vector3(
    //     ((transform.GetTranslation().x * screen_scale.x) + (model_2d.GetScale().x / 2.0)),
    //     ((transform.GetTranslation().y * screen_scale.y) + (model_2d.GetScale().y / 2.0)),
    //     0.0
    // ));

    // model_2d.SetRotation(transform.GetRotation());

    Vector2 viewport(camera->GetWidth(), camera->GetHeight());

    SetUniform("Viewport", viewport);
    SetUniform("u_modelMatrix", model_2d.GetMatrix());
}
} // namespace hyperion
