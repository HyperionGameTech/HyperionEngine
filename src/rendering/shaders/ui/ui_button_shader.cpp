#include "ui_button_shader.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../../util/shader_preprocessor.h"

namespace hyperion {
UIButtonShader::UIButtonShader(const ShaderProperties &properties)
    : UIObjectShader(properties)
{
    const std::string fs_path("shaders/ui/ui_button.frag");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_FRAGMENT,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
        properties,
        fs_path
    );
}
} // namespace hyperion
