/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <generator/Generator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <Core/logging/Logger.hpp>

#include <Core/io/ByteWriter.hpp>

namespace Hyperion {
namespace CodeGen {

HYP_DECLARE_LOG_CHANNEL(Tool);

Result GeneratorBase::Generate(const Analyzer& analyzer, const Module& mod) const
{
    const FilePath outputFilePath = GetOutputFilePath(analyzer, mod);

    if (outputFilePath.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Output file path is empty");
    }

    const FilePath basePath = outputFilePath.BasePath();

    if (!basePath.IsDirectory())
    {
        basePath.MkDir();
    }

    if (!basePath.IsDirectory())
    {
        HYP_LOG(Tool, Error, "Failed to create output directory: {}", basePath);

        return HYP_MAKE_ERROR(Error, "Failed to create output directory");
    }

    MemoryByteWriter memoryWriter;

    Result res = Generate(analyzer, mod, memoryWriter);

    if (!res.HasError() && memoryWriter.Position() > 0)
    {
        FileByteWriter fileWriter { outputFilePath };
        fileWriter.Write(memoryWriter.GetBuffer());
    }

    return res;
}

} // namespace CodeGen
} // namespace Hyperion