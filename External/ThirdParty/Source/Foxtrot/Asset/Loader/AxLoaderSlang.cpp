#include "AxLoaderSlang.hpp"

#include <ThirdParty/Slang/slang.h>

#include <Core/FxFile.hpp>

using Status = AxLoaderSlang::Status;

// static EShLanguage RxShaderTypeToShLanguage(RxShaderType type)
// {
//     switch (type) {
//     case RxShaderType::Fragment:
//         return EShLangFragment;
//     case RxShaderType::Vertex:
//         return EShLangVertex;
//     default:
//         FxLogError("Unknown shader type!");
//         return EShLangVertex;
//     }
// }

Status AxLoaderSlang::LoadFromFile(FxTSRef<AxBase> asset, const std::string& path)
{
    // FxRef<FxAssetShader> shader(asset);

    // FxFile shader_file(path.c_str(), FxFile::eRead, FxFile::eText);

    // glslang::TShader glsl_shader(RxShaderTypeToShLanguage(shader->ShaderType));

    // FxSlice<char> file_data = shader_file.Read();
    // glsl_shader.setStrings(&file_data.Ptr, 1);

    // if (!glsl_shader.parse(GetResources(), 450, false, EShMsgDefault)) {
    // }


    // glsl_shader.addSourceText(file_data.Ptr, file_data.Size);

    // shader_file.Close();

    return Status::eSuccess;
}

Status AxLoaderSlang::LoadFromMemory(FxTSRef<AxBase> asset, const uint8* data, uint32 size) { return Status::eSuccess; }

void AxLoaderSlang::CreateGpuResource(FxTSRef<AxBase>& asset) {}

void AxLoaderSlang::Destroy(FxTSRef<AxBase>& asset) {}
