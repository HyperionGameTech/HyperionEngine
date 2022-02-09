#include "terrain_shader.h"
#include "../rendering/environment.h"
#include "../asset/asset_manager.h"
#include "../asset/text_loader.h"
#include "../util/shader_preprocessor.h"

namespace hyperion {
TerrainShader::TerrainShader(const ShaderProperties &properties)
    : LightingShader(properties)
{
    const std::string fs_path("shaders/terrain.frag");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_FRAGMENT,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
        properties,
        fs_path
    );

    SetUniform("BaseTerrainScale", 0.03f);
    SetUniform("Level1Scale", 1.0f);
    SetUniform("Level1Height", 250.0f);
    SetUniform("SlopeScale", 1.0f);
}

void TerrainShader::ApplyMaterial(const Material &mat)
{
    LightingShader::ApplyMaterial(mat);
}
} // namespace hyperion
