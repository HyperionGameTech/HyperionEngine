/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <generator/generators/CSharpModuleGenerator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <asset/ByteWriter.hpp>

namespace hyperion {
namespace buildtool {
    
FilePath CSharpModuleGenerator::GetOutputFilePath(const Analyzer &analyzer, const Module &mod) const
{
    FilePath relative_path = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

    return analyzer.GetCSharpOutputDirectory() / relative_path.BasePath() / StringUtil::StripExtension(relative_path.Basename()) + ".cs";
}

Result<void> CSharpModuleGenerator::Generate_Internal(const Analyzer &analyzer, const Module &mod, ByteWriter &writer) const
{
    return { };
}

} // namespace buildtool
} // namespace hyperion