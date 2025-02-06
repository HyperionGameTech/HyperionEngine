#include <core/cli/CommandLine.hpp>

#include <core/logging/Logger.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/functional/Proc.hpp>

#include <core/memory/UniquePtr.hpp>

#include <driver/Driver.hpp>
#include <driver/clang/ClangDriver.hpp>

#include <generator/generators/CXXModuleGenerator.hpp>
#include <generator/generators/CSharpModuleGenerator.hpp>

#include <parser/Parser.hpp>

#include <analyzer/Analyzer.hpp>

namespace hyperion {
namespace buildtool {

HYP_DEFINE_LOG_CHANNEL(BuildTool);

class HypBuildTool
{
public:
    HypBuildTool(
        UniquePtr<IDriver> &&driver,
        const FilePath &working_directory,
        const FilePath &source_directory,
        const FilePath &cxx_output_directory,
        const FilePath &csharp_output_directory,
        const HashSet<FilePath> &exclude_directories,
        const HashSet<FilePath> &exclude_files
    ) : m_driver(std::move(driver))
    {
        m_analyzer.SetWorkingDirectory(working_directory);
        m_analyzer.SetSourceDirectory(source_directory);
        m_analyzer.SetCXXOutputDirectory(cxx_output_directory);
        m_analyzer.SetCSharpOutputDirectory(csharp_output_directory);

        m_analyzer.SetExcludeDirectories(exclude_directories);
        m_analyzer.SetExcludeFiles(exclude_files);

        m_analyzer.SetGlobalDefines(GetGlobalDefines());
        m_analyzer.SetIncludePaths(GetIncludePaths());
    }

    ~HypBuildTool()
    {
    }

    Result<void> Run()
    {
        FindModules();

        ProcessModules();

        for (const UniquePtr<Module> &mod : m_analyzer.GetModules()) {
            for (const Pair<String, HypClassDefinition> &hyp_class : mod->GetHypClasses()) {
                HYP_LOG(BuildTool, Info, "Class: {}", hyp_class.first);

                // Log out all members
                for (const HypMemberDefinition &hyp_member : hyp_class.second.members) {
                    if (!hyp_member.cxx_type) {
                        continue;
                    }

                    json::JSONValue json;
                    hyp_member.cxx_type->ToJSON(json);

                    HYP_LOG(BuildTool, Info, "\tMember: {}\t{}", hyp_member.name, json.ToString(true));
                }
            }
        }

        GenerateOutputFiles();

        HYP_LOG(BuildTool, Info, "Build tool finished");

        if (m_analyzer.GetState().HasErrors()) {
            for (const AnalyzerError &error : m_analyzer.GetState().errors) {
                HYP_LOG(BuildTool, Error, "Error: {}\t{}", error.GetMessage(), error.GetErrorMessage());
            }

            return HYP_MAKE_ERROR(Error, "Build tool finished with errors");
        }

        return { };
    }

private:
    HashMap<String, String> GetGlobalDefines() const
    {
        return {
            { "HYP_BUILDTOOL", "1" },
            { "HYP_VULKAN", "1" },
            { "HYP_CLASS(...)", "" },
            { "HYP_STRUCT(...)", "" },
            { "HYP_ENUM(...)", "" },
            { "HYP_FIELD(...)", "" },
            { "HYP_METHOD(...)", "" },
            { "HYP_PROPERTY(...)", "" },
            { "HYP_CONSTANT(...)", "" },
            { "HYP_OBJECT_BODY(...)", "" },
            { "HYP_API", "" },
            { "HYP_EXPORT", "" },
            { "HYP_IMPORT", "" },
            { "HYP_FORCE_INLINE", "inline" },
            { "HYP_NODISCARD", "" }
        };
    }

    HashSet<String> GetIncludePaths() const
    {
        const FilePath &working_directory = m_analyzer.GetWorkingDirectory();

        return {
            working_directory / "src",
            working_directory / "include"
        };
    }

    void FindModules()
    {
        HYP_LOG(BuildTool, Info, "Finding modules...");

        Proc<void, const FilePath &> IterateFilesAndSubdirectories;

        IterateFilesAndSubdirectories = [&](const FilePath &dir)
        {
            for (const String &exclude_dir : m_analyzer.GetExcludeDirectories()) {
                const FilePath relative_path = FilePath(FileSystem::RelativePath(dir.Data(), m_analyzer.GetSourceDirectory().Data()).c_str());

                if (relative_path.StartsWith(exclude_dir)) {
                    return;
                }
            }

            Array<Module *> local_modules;
            
            for (const FilePath &file : dir.GetAllFilesInDirectory()) {
                if (file.EndsWith(".hpp")) {
                    const FilePath relative_path = FilePath(FileSystem::RelativePath(file.Data(), m_analyzer.GetSourceDirectory().Data()).c_str());

                    if (m_analyzer.GetExcludeFiles().Contains(relative_path)) {
                        continue;
                    }

                    local_modules.PushBack(m_analyzer.AddModule(file));
                }
            }

            Array<FilePath> local_subdirectories = dir.GetSubdirectories();

            if (local_subdirectories.Empty()) {
                return;
            }

            for (const FilePath &subdirectory : local_subdirectories) {
                IterateFilesAndSubdirectories(subdirectory);
            }
        };

        IterateFilesAndSubdirectories(m_analyzer.GetSourceDirectory());

        HYP_LOG(BuildTool, Info, "Found {} total modules", m_analyzer.GetModules().Size());
    }

    void ProcessModules()
    {
        HYP_LOG(BuildTool, Info, "Processing modules...");

        for (const UniquePtr<Module> &mod : m_analyzer.GetModules()) {
            HYP_LOG(BuildTool, Info, "Processing module: {}", mod->GetPath());

            auto result = m_driver->ProcessModule(m_analyzer, *mod);

            if (result.HasError()) {
                m_analyzer.AddError(result.GetError());
            }
        }
    }

    void GenerateOutputFiles()
    {
        HYP_LOG(BuildTool, Info, "Generating output files...");

        RC<CXXModuleGenerator> cxx_module_generator = MakeRefCountedPtr<CXXModuleGenerator>();
        RC<CSharpModuleGenerator> csharp_module_generator = MakeRefCountedPtr<CSharpModuleGenerator>();

        for (const UniquePtr<Module> &mod : m_analyzer.GetModules()) {
            if (mod->GetHypClasses().Empty()) {
                continue;
            }
            
            HYP_LOG(BuildTool, Info, "Generating output files for module: {}", mod->GetPath());

            if (Result<void> res = cxx_module_generator->Generate(m_analyzer, *mod); res.HasError()) {
                m_analyzer.AddError(AnalyzerError(res.GetError(), mod->GetPath()));
            }

            if (Result<void> res = csharp_module_generator->Generate(m_analyzer, *mod); res.HasError()) {
                m_analyzer.AddError(AnalyzerError(res.GetError(), mod->GetPath()));
            }
        }
    }

    UniquePtr<IDriver>  m_driver;
    Analyzer            m_analyzer;
};

} // namespace buildtool
} // namespace hyperion

using namespace hyperion;
using namespace buildtool;

int main(int argc, char **argv)
{
    CommandLineParser arg_parse {
        CommandLineArgumentDefinitions()
            .Add("WorkingDirectory", "", "", CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING)
            .Add("SourceDirectory", "", "", CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING)
            .Add("CXXOutputDirectory", "", "", CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING)
            .Add("CSharpOutputDirectory", "", "", CommandLineArgumentFlags::REQUIRED, CommandLineArgumentType::STRING)
            .Add("ExcludeDirectories", "", "", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING)
            .Add("ExcludeFiles", "", "", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING)
            .Add("Mode", "m", "", CommandLineArgumentFlags::NONE, Array<String> { "ParseHeaders" }, String("ParseHeaders"))
    };

    if (auto parse_result = arg_parse.Parse(argc, argv)) {
        const FilePath working_directory = FilePath(parse_result.GetValue()["WorkingDirectory"].AsString());

        if (!working_directory.IsDirectory()) {
            HYP_LOG(BuildTool, Error, "Working directory is not a directory: {}", working_directory);

            return 1;
        }

        const FilePath source_directory = FilePath(parse_result.GetValue()["SourceDirectory"].AsString());

        if (!source_directory.IsDirectory()) {
            HYP_LOG(BuildTool, Error, "Source directory is not a directory: {}", source_directory);

            return 1;
        }

        const FilePath cxx_output_directory = FilePath(parse_result.GetValue()["CXXOutputDirectory"].AsString());
        const FilePath csharp_output_directory = FilePath(parse_result.GetValue()["CSharpOutputDirectory"].AsString());

        HashSet<FilePath> exclude_directories;
        HashSet<FilePath> exclude_files;

        if (parse_result.GetValue().Contains("ExcludeDirectories")) {
            const json::JSONArray exclude_directories_json = parse_result.GetValue()["ExcludeDirectories"].ToArray();

            for (const json::JSONValue &value : exclude_directories_json) {
                exclude_directories.Insert(FilePath(FileSystem::RelativePath(value.ToString().Data(), source_directory.Data()).c_str()));
            }
        }

        if (parse_result.GetValue().Contains("ExcludeFiles")) {
            const json::JSONArray exclude_files_json = parse_result.GetValue()["ExcludeFiles"].ToArray();

            for (const json::JSONValue &value : exclude_files_json) {
                exclude_files.Insert(FilePath(FileSystem::RelativePath(value.ToString().Data(), source_directory.Data()).c_str()));
            }
        }

        buildtool::HypBuildTool build_tool {
            MakeUnique<ClangDriver>(),
            working_directory,
            source_directory,
            cxx_output_directory,
            csharp_output_directory,
            exclude_directories,
            exclude_files
        };
        
        Result<void> res = build_tool.Run();

        if (res.HasError()) {
            return 1;
        }
    } else {
        HYP_LOG(BuildTool, Error, "Failed to parse arguments!\n\t{}", parse_result.GetError().GetMessage());

        return 1;
    }

    return 0;
}
