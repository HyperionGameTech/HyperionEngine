/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <generator/generators/CSharpModuleGenerator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <asset/ByteWriter.hpp>

namespace hyperion {
namespace buildtool {
    
FilePath CSharpModuleGenerator::GetOutputFilePath(const Analyzer &analyzer, const Module &mod) const
{
    return analyzer.GetCSharpOutputDirectory() / mod.GetPath().Basename();
}

Result<void> CSharpModuleGenerator::Generate_Internal(const Analyzer &analyzer, const Module &mod, ByteWriter &writer) const
{
    return HYP_MAKE_ERROR(Error, "Not implemented");
}

} // namespace buildtool
} // namespace hyperion