/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <generator/Generator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <core/logging/Logger.hpp>

#include <core/io/ByteWriter.hpp>

namespace hyperion {
namespace buildtool {

HYP_DECLARE_LOG_CHANNEL(BuildTool);

Result GeneratorBase::Generate(const Analyzer& analyzer, const Module& mod) const
{
    const FilePath output_file_path = GetOutputFilePath(analyzer, mod);

    if (output_file_path.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Output file path is empty");
    }

    const FilePath base_path = output_file_path.BasePath();

    if (!base_path.IsDirectory())
    {
        base_path.MkDir();
    }

    if (!base_path.IsDirectory())
    {
        HYP_LOG(BuildTool, Error, "Failed to create output directory: {}", base_path);

        return HYP_MAKE_ERROR(Error, "Failed to create output directory");
    }

    MemoryByteWriter memory_writer;

    Result res = Generate_Internal(analyzer, mod, memory_writer);

    if (!res.HasError())
    {
        FileByteWriter file_writer { output_file_path };
        file_writer.Write(memory_writer.GetBuffer());
    }

    return res;
}

} // namespace buildtool
} // namespace hyperion