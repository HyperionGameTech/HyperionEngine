#pragma once

#include <Asset/FxDataPack.hpp>
#include <Core/FxSizedArray.hpp>

struct FxShaderMacro
{
    const char* pcName;
    const char* pcValue;
};

class FxShaderCompiler
{
public:
    enum class Result
    {
        eSuccess,
        eFailed,
    };

public:
    FxShaderCompiler() = default;


    static void CompileAllShaders(const char* folder_path);

    static Result Compile(const char* path, FxDataPack& pack, const FxSizedArray<FxShaderMacro>& macros,
                          bool do_db_flush = true);


    static bool CompileIfOutOfDate(const char* path, FxDataPack& pack, const FxSizedArray<FxShaderMacro>& macros);

    static bool IsOutOfDate(const char* path);

    static void Destroy();
};
