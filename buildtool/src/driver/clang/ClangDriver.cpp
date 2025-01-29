/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <driver/clang/ClangDriver.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <util/json/JSON.hpp>

#include <core/utilities/Format.hpp>
#include <core/utilities/StringView.hpp>

#include <core/logging/Logger.hpp>

#include <asset/BufferedByteReader.hpp>

#include <stdio.h>
#include <unistd.h>

namespace hyperion {
namespace buildtool {

using namespace json;

class TmpFile
{
public:
    TmpFile()
        : m_file(tmpfile())
    {
    }

    TmpFile(const TmpFile &)            = delete;
    TmpFile &operator=(const TmpFile &) = delete;

    TmpFile(TmpFile &&other)
        : m_file(other.m_file)
    {
        other.m_file = nullptr;
    }

    TmpFile &operator=(TmpFile &&other)
    {
        if (this != &other) {
            if (m_file) {
                fclose(m_file);
            }

            m_file = other.m_file;
            other.m_file = nullptr;
        }

        return *this;
    }

    ~TmpFile()
    {
        if (m_file) {
            fclose(m_file);
        }
    }

    FILE *GetFile() const
    {
        return m_file;
    }

    TmpFile &operator<<(UTF8StringView str)
    {
        if (m_file) {
            fwrite(str.Data(), 1, str.Size(), m_file);
        }

        return *this;
    }

private:
    FILE    *m_file;
};

// @TODO:
// 1. Loop through regions of interest in the module file (all HYP_CLASS, HYP_STRUCT, HYP_ENUM etc)
// 2. Select all the class / struct / enum content by counting braces.
// 3. Output the selected content to a temporary file.
// 4. Run clang on the temporary file.

static Result<JSONValue, AnalyzerError> ReadModuleAST(const Analyzer &analyzer, Module &mod)
{
    String defines_string;

    for (const Pair<String, String> &define : analyzer.GetGlobalDefines()) {
        defines_string += HYP_FORMAT(" -D'{}={}'", define.first, define.second);
    }

    String command = HYP_FORMAT("clang{} -std=c++20 -Xclang -ast-dump=json -fsyntax-only {}",
        defines_string, mod.GetPath());

    HYP_LOG(BuildTool, Info, "Running clang: {}", command);

    FILE *file = popen(command.Data(), "rb");

    if (!file) {
        return HYP_MAKE_ERROR(AnalyzerError, "Failed to run clang", mod.GetPath(), errno);
    }

    BufferedReader reader(
        MakeRefCountedPtr<FileBufferedReaderSource>(
            file,
            [](FILE *file)
            {
                return pclose(file);
            }
        )
    );
    
    if (reader.Eof()) {
        HYP_BREAKPOINT;

        return HYP_MAKE_ERROR(AnalyzerError, "Failed to read output", mod.GetPath());
    }

    auto json_parse_result = JSON::Parse(reader);

    if (!json_parse_result.ok) {
        return HYP_MAKE_ERROR(AnalyzerError, "Failed to parse JSON output", mod.GetPath());
    }

    return json_parse_result.value;
}

Result<void, AnalyzerError> ClangDriver::ProcessModule(const Analyzer &analyzer, Module &mod)
{
    auto ast_result = ReadModuleAST(analyzer, mod);

    if (ast_result.HasError()) {
        HYP_LOG(BuildTool, Error, "Failed to read module AST: {}\tError code: {}", ast_result.GetError().GetMessage(), ast_result.GetError().GetErrorCode());

        HYP_BREAKPOINT;

        return ast_result.GetError();
    }

    return HYP_MAKE_ERROR(AnalyzerError, "Not implemented", mod.GetPath());
}

} // namespace buildtool
} // namespace hyperion