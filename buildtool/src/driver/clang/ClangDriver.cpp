/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <driver/clang/ClangDriver.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <util/json/JSON.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>

#include <asset/BufferedByteReader.hpp>

#include <stdio.h>
#include <unistd.h>

namespace hyperion {
namespace buildtool {

using namespace json;

static Result<JSONValue, AnalyzerError> ReadModuleAST(const Analyzer &analyzer, Module &mod)
{
    String defines_string;

    for (const Pair<String, String> &define : analyzer.GetGlobalDefines()) {
        defines_string += HYP_FORMAT("-D'{}={}' ", define.first, define.second);
    }

    String includes_string;

    for (const String &include : analyzer.GetIncludePaths()) {
        includes_string += HYP_FORMAT("-I'{}' ", include);
    }

    String command = HYP_FORMAT("cd {} && clang {} {} -std=c++20 -Xclang -ast-dump=json -fsyntax-only {}",
        analyzer.GetWorkingDirectory(),
        includes_string,
        defines_string,
        mod.GetPath());

    HYP_LOG(BuildTool, Info, "Running clang: {}", command);

    FILE *file = popen(command.Data(), "r");

    if (!file) {
        return HYP_MAKE_ERROR(AnalyzerError, "Failed to run clang", mod.GetPath());
    }

    BufferedReader reader(
        MakeRefCountedPtr<FileBufferedReaderSource>(
            file,
            [](FILE *file)
            {
                pclose(file);
            }
        )
    );
    
    if (reader.Eof()) {
        return HYP_MAKE_ERROR(AnalyzerError, "Failed to read output", mod.GetPath());
    }

    auto json_parse_result = JSON::Parse(reader);

    HYP_BREAKPOINT;

    if (!json_parse_result.ok) {
        return HYP_MAKE_ERROR(AnalyzerError, "Failed to parse JSON output", mod.GetPath());
    }

    return json_parse_result.value;
}

Result<void, AnalyzerError> ClangDriver::ProcessModule(const Analyzer &analyzer, Module &mod)
{
    auto ast_result = ReadModuleAST(analyzer, mod);

    if (ast_result.HasError()) {
        HYP_BREAKPOINT;

        return ast_result.GetError();
    }

    return HYP_MAKE_ERROR(AnalyzerError, "Not implemented", mod.GetPath());
}

} // namespace buildtool
} // namespace hyperion