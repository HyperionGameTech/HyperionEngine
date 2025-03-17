#include <core/cli/CommandLine.hpp>

#include <core/logging/Logger.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>
#include <core/threading/Mutex.hpp>

#include <core/functional/Proc.hpp>

#include <core/memory/UniquePtr.hpp>

#include <generator/generators/CXXModuleGenerator.hpp>
#include <generator/generators/CSharpModuleGenerator.hpp>

#include <parser/Parser.hpp>

#include <analyzer/Analyzer.hpp>

namespace hyperion {
namespace buildtool {

HYP_DEFINE_LOG_CHANNEL(BuildTool);

class WorkerThread : public TaskThread
{
public:
    WorkerThread(ThreadID id)
        : TaskThread(id)
    {
    }

    virtual ~WorkerThread() override = default;
};

class WorkerThreadPool : public TaskThreadPool
{
public:
    WorkerThreadPool()
        : TaskThreadPool(TypeWrapper<WorkerThread>(), "BuildTool_WorkerThread", 4)
    {
    }

    virtual ~WorkerThreadPool() override = default;
};

class HypBuildTool
{
public:
    HypBuildTool(
        const FilePath &working_directory,
        const FilePath &source_directory,
        const FilePath &cxx_output_directory,
        const FilePath &csharp_output_directory,
        const HashSet<FilePath> &exclude_directories,
        const HashSet<FilePath> &exclude_files
    )
    {
        m_analyzer.SetWorkingDirectory(working_directory);
        m_analyzer.SetSourceDirectory(source_directory);
        m_analyzer.SetCXXOutputDirectory(cxx_output_directory);
        m_analyzer.SetCSharpOutputDirectory(csharp_output_directory);

        m_analyzer.SetExcludeDirectories(exclude_directories);
        m_analyzer.SetExcludeFiles(exclude_files);

        m_analyzer.SetGlobalDefines(GetGlobalDefines());
        m_analyzer.SetIncludePaths(GetIncludePaths());

        m_thread_pool.Start();
    }

    ~HypBuildTool()
    {
        m_thread_pool.Stop();
    }

    Result Run()
    {
        FindModules();

        Task process_modules = ProcessModules();
        WaitWhileTaskRunning(process_modules);

        Task generate_output_files = GenerateOutputFiles();
        WaitWhileTaskRunning(generate_output_files);

        if (m_analyzer.GetState().HasErrors()) {
            for (const AnalyzerError &error : m_analyzer.GetState().errors) {
                HYP_LOG(BuildTool, Error, "Error: {}", error.GetMessage());
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
    }

    Task<void> ProcessModules()
    {
        Task<void> task;

        TaskBatch *batch = new TaskBatch();
        batch->pool = &m_thread_pool;
        batch->OnComplete.Bind([batch, task_executor = task.Initialize()]()
        {
            task_executor->Fulfill();
            
            DeferDeleteTaskBatch(batch);
        }).Detach();

        for (const UniquePtr<Module> &mod : m_analyzer.GetModules()) {
            batch->AddTask([this, mod = mod.Get()]()
            {
                auto result = m_analyzer.ProcessModule(*mod);

                if (result.HasError()) {
                    m_analyzer.AddError(result.GetError());

                    return;
                }
            });
        }

        TaskSystem::GetInstance().EnqueueBatch(batch);

        return task;
    }

    Task<void> GenerateOutputFiles()
    {
        Task<void> task;

        TaskBatch *batch = new TaskBatch();
        batch->pool = &m_thread_pool;
        batch->OnComplete.Bind([batch, task_executor = task.Initialize()]()
        {
            task_executor->Fulfill();

            DeferDeleteTaskBatch(batch);
        }).Detach();

        RC<CXXModuleGenerator> cxx_module_generator = MakeRefCountedPtr<CXXModuleGenerator>();
        RC<CSharpModuleGenerator> csharp_module_generator = MakeRefCountedPtr<CSharpModuleGenerator>();

        for (const UniquePtr<Module> &mod : m_analyzer.GetModules()) {
            if (mod->GetHypClasses().Empty()) {
                continue;
            }

            batch->AddTask([this, cxx_module_generator, csharp_module_generator, mod = mod.Get()]()
            {
                if (Result res = cxx_module_generator->Generate(m_analyzer, *mod); res.HasError()) {
                    m_analyzer.AddError(AnalyzerError(res.GetError(), mod->GetPath()));
                }

                if (Result res = csharp_module_generator->Generate(m_analyzer, *mod); res.HasError()) {
                    m_analyzer.AddError(AnalyzerError(res.GetError(), mod->GetPath()));
                }
            });
        }

        TaskSystem::GetInstance().EnqueueBatch(batch);

        return task;
    }

    void LogGeneratedClasses() const
    {
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
    }

    void WaitWhileTaskRunning(const Task<void> &task)
    {
        Threads::AssertOnThread(g_main_thread);

        AssertThrow(task.IsValid());

        while (!task.IsCompleted()) {
            Threads::Sleep(100);
        }
    }
    
    // Setup a task to delete the batch
    // Use when deleting the batch inline would cause an issue due to destructing a mutex that is currently locked.
    static void DeferDeleteTaskBatch(TaskBatch *batch)
    {
        if (!batch) {
            return;
        }

        TaskSystem::GetInstance().Enqueue([batch]()
        {
            delete batch;
        }, TaskThreadPoolName::THREAD_POOL_BACKGROUND, TaskEnqueueFlags::FIRE_AND_FORGET);
    }

    WorkerThreadPool    m_thread_pool;
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
        TaskSystem::GetInstance().Start();

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
            working_directory,
            source_directory,
            cxx_output_directory,
            csharp_output_directory,
            exclude_directories,
            exclude_files
        };
        
        Result res = build_tool.Run();

        TaskSystem::GetInstance().Stop();

        if (res.HasError()) {
            return 1;
        }
    } else {
        HYP_LOG(BuildTool, Error, "Failed to parse arguments!\n\t{}", parse_result.GetError().GetMessage());

        return 1;
    }

    return 0;
}
