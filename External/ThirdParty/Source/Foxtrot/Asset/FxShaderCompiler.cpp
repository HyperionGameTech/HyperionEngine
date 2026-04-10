#include "FxShaderCompiler.hpp"

#include <Core/FxDefines.hpp>

// #include <ThirdParty/Slang/slang-com-ptr.h>
// #include <ThirdParty/Slang/slang.h>

#include "FxShaderPreproc.hpp"

// DXC
#ifndef FX_PLATFORM_WINDOWS
#include <dxc/WinAdapter.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atlbase.h>
#endif
#include <dxc/dxcapi.h>

#include <Asset/FxDataPack.hpp>
#include <Core/FxBasicDb.hpp>
#include <Core/FxFile.hpp>
#include <Core/FxFilesystemIO.hpp>
#include <Core/FxStackArray.hpp>
#include <Renderer/Backend/RxShader.hpp>

static FxBasicDb sShaderCompileDb;

using CompileResult = FxShaderCompiler::Result;

static bool IsShaderUpToDate(RxShaderId entry_id, const char* path)
{
    FxBasicDbEntry* entry = sShaderCompileDb.FindEntry(entry_id);
    if (!entry) {
        FxLogWarning("Cannot find entry for shader!");
        return false;
    }

    uint64 latest_modification_time = FxFilesystemIO::FileGetLastModified(path);

    if (std::stoull(entry->Value) < latest_modification_time) {
        return false;
    }

    return true;
}


bool FxShaderCompiler::IsOutOfDate(const char* path)
{
    if (!sShaderCompileDb.IsOpen()) {
        sShaderCompileDb.Open(FX_BASE_DIR "/Shaders/LastUpdated.fxd");
    }

    RxShaderId compile_entry_id = FxHashStr64(path);

    return !IsShaderUpToDate(compile_entry_id, path);
}

bool FxShaderCompiler::CompileIfOutOfDate(const char* path, FxDataPack& pack, const FxSizedArray<FxShaderMacro>& macros)
{
    bool out_of_date = IsOutOfDate(path);

    if (out_of_date) {
        Compile(path, pack, macros);
    }

    return out_of_date;
}

#define SHADER_VERSION L"6_7"

static const wchar_t* ShaderTypeToDxName(RxShaderType type)
{
    switch (type) {
    case RxShaderType::eVertex:
        return L"vs_" SHADER_VERSION;
    case RxShaderType::eFragment:
        return L"ps_" SHADER_VERSION;
    }
    return L"";
}

struct CompileState
{
    const char* pcPath;
    FxDataPack& Pack;
    const FxSizedArray<FxShaderMacro>& pcMacros;
    FxShaderPreproc::Result& Preproc;
    CComPtr<IDxcUtils> pUtils;
    CComPtr<IDxcCompiler3> pCompiler;
    CComPtr<IDxcIncludeHandler> pIncludeHandler;
};


static CompileResult CompileProgram(const CompileState& state, RxShaderType shader_type)
{
    constexpr uint32 cCodePage = DXC_CP_UTF8;


    // Convert path to a wide char string
    wchar_t wpath[128];
    mbstowcs(wpath, state.pcPath, 128);

    const FxSizedArray<LPCWSTR> compile_args = {
        wpath,
        // Entrypoint
        L"-E",
        L"main",

        // Target profile
        L"-T",
        ShaderTypeToDxName(shader_type),

        // Output format
        L"-spirv",
    };

    CComPtr<IDxcBlobEncoding> source_blob;
    auto& shader_raw_data = state.Preproc.GetBuffer(shader_type);
    state.pUtils->CreateBlob(shader_raw_data.pData, shader_raw_data.Size, cCodePage, &source_blob);

    DxcBuffer buffer {};
    buffer.Encoding = cCodePage;
    buffer.Ptr = source_blob->GetBufferPointer();
    buffer.Size = source_blob->GetBufferSize();

    HRESULT hresult;

    // printf("CODE:\n%.*s\n", shader_raw_data.Size, shader_raw_data.pData);

    CComPtr<IDxcResult> result { nullptr };
    hresult = state.pCompiler->Compile(&buffer, compile_args.pData, static_cast<uint32>(compile_args.Size),
                                       state.pIncludeHandler, IID_PPV_ARGS(&result));

    if (SUCCEEDED(hresult)) {
        result->GetStatus(&hresult);
    }

    if (FAILED(hresult) && result) {
        CComPtr<IDxcBlobEncoding> error_blob;
        hresult = result->GetErrorBuffer(&error_blob);

        if (SUCCEEDED(hresult) && error_blob) {
            FxLogError("Failed to compile shader '{}'!", state.pcPath);
            FxLogError("Err: {}", reinterpret_cast<const char*>(error_blob->GetBufferPointer()));
            return CompileResult::eFailed;
        }
    }

    CComPtr<IDxcBlob> spirv_bin;
    result->GetResult(&spirv_bin);

    const RxShaderId shader_id = RxShader::GenerateShaderId(shader_type, state.pcMacros);

    FxLogInfo("IS 4 byte aligned? {:s}", !(spirv_bin->GetBufferSize() % 4));

    FxLogInfo("Writing shader '{}' (Id={:x}) to data pack!", state.pcPath, shader_id);

    state.Pack.AddEntry(shader_id, FxMakeSlice<uint8>(reinterpret_cast<uint8*>(spirv_bin->GetBufferPointer()),
                                                      spirv_bin->GetBufferSize()));


    return CompileResult::eSuccess;
}

#define SOURCE_EXISTS(_shader_type) (preproc.GetBuffer(_shader_type).Size > 0)
#define COMPILE_DEFAULT(_shader_type)                                                                                  \
    CompileProgram(CompileState(path, pack, macros, preproc, utils, compiler, include_handler), _shader_type)

#define TRY_COMPILE_PROGRAM(_shader_type)                                                                              \
    if (SOURCE_EXISTS(_shader_type) && COMPILE_DEFAULT(_shader_type) != CompileResult::eSuccess) {                     \
        return CompileResult::eFailed;                                                                                 \
    }


FxShaderCompiler::Result FxShaderCompiler::Compile(const char* path, FxDataPack& pack,
                                                   const FxSizedArray<FxShaderMacro>& macros, bool do_db_flush)
{
    FxLogInfo("Compiling shader {} with {} macros", path, macros.Size);

    FxFile file(path, FxFile::ModType::eRead, FxFile::DataType::eBinary);
    FxSlice<char> file_data = file.Read<char>();

    FxShaderPreproc::Result preproc = FxShaderPreproc::Process(file_data, macros);

    CComPtr<IDxcUtils> utils;
    CComPtr<IDxcCompiler3> compiler;

    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

    CComPtr<IDxcIncludeHandler> include_handler;
    utils->CreateDefaultIncludeHandler(&include_handler);

    TRY_COMPILE_PROGRAM(RxShaderType::eVertex);
    TRY_COMPILE_PROGRAM(RxShaderType::eFragment);

    return CompileResult::eSuccess;
}

void FxShaderCompiler::Destroy() { sShaderCompileDb.Close(); }
