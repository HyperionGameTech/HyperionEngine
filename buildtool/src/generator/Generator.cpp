/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <generator/Generator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <asset/ByteWriter.hpp>

namespace hyperion {
namespace buildtool {
    
Result<void> GeneratorBase::Generate(const Analyzer &analyzer, const Module &mod) const
{
    const FilePath output_file_path = GetOutputFilePath(analyzer, mod);

    if (output_file_path.Empty()) {
        return HYP_MAKE_ERROR(Error, "Output file path is empty");
    }

    const FilePath base_path = output_file_path.BasePath();

    if (!base_path.Exists()) {
        base_path.MkDir();
    }

    if (!base_path.Exists()) {
        return HYP_MAKE_ERROR(Error, "Failed to create output directory");
    }

    FileByteWriter writer { output_file_path };

    return Generate_Internal(analyzer, mod, writer);
}

} // namespace buildtool
} // namespace hyperion