#include <core/system/ArgParse.hpp>

#include <core/logging/Logger.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>
#include <core/threading/Mutex.hpp>


namespace hyperion {
HYP_DEFINE_LOG_CHANNEL(BuildTool);

namespace buildtool {

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
    {
        CreateThreads<WorkerThread>(NAME("BuildTool_WorkerThread"), 4);
    }

    virtual ~WorkerThreadPool() override = default;
};

class HypBuildTool
{
public:
    HypBuildTool(const FilePath &working_directory)
        : m_working_directory(working_directory)
    {
        m_thread_pool.Start();
    }

    ~HypBuildTool()
    {
        m_thread_pool.Stop();
    }

    void Run()
    {
        Task<void> task = ParseHeaders();
        WaitWhileTaskRunning(task);
    }

private:
    Task<void> ParseHeaders()
    {
        HYP_LOG(BuildTool, LogLevel::INFO, "Parsing headers...");

        return TaskSystem::GetInstance().Enqueue([this]()
        {
            Array<FilePath> files;
            Mutex mutex;

            Proc<void, const FilePath &> IterateFilesAndSubdirectories;

            IterateFilesAndSubdirectories = [&](const FilePath &dir)
            {
                Array<FilePath> local_files = dir.GetAllFilesInDirectory();
                Array<FilePath> local_subdirectories = dir.GetSubdirectories();
                Array<Task<void>> local_subtasks;

                HYP_LOG(BuildTool, LogLevel::INFO, "On worker thread {}: {}: Discovered {} files, {} subdirectories",
                    ThreadID::Current().name, dir, local_files.Size(), local_subdirectories.Size());

                {
                    Mutex::Guard guard(mutex);
                    files.Concat(local_files);
                }

                if (local_subdirectories.Empty()) {
                    return;
                }

                for (const FilePath &subdirectory : local_subdirectories) {
                    local_subtasks.PushBack(m_thread_pool.Enqueue([&IterateFilesAndSubdirectories, subdirectory]()
                    {
                        IterateFilesAndSubdirectories(subdirectory);
                    }));
                }

                AwaitAll(local_subtasks.ToSpan());
            };

            IterateFilesAndSubdirectories(m_working_directory);

            HYP_LOG(BuildTool, LogLevel::INFO, "Found {} total files", files.Size());
        });
    }

    void WaitWhileTaskRunning(Task<void> &task)
    {
        Threads::AssertOnThread(ThreadName::THREAD_MAIN);

        if (!task.IsValid()) {
            return;
        }

        while (!task.IsCompleted()) {
            std::printf(".");
            
            Threads::Sleep(100);
        }

        // Newline
        std::puts("");
    }

    FilePath            m_working_directory;
    WorkerThreadPool    m_thread_pool;
};

} // namespace buildtool
} // namespace hyperion

using namespace hyperion;

int main(int argc, char **argv)
{
    ArgParse arg_parse;
    arg_parse.Add("WorkingDirectory", "w", ArgFlags::NONE, CommandLineArgumentType::STRING, FilePath::Current().BasePath());
    arg_parse.Add("Mode", "m", ArgFlags::NONE, Array<String> { "ParseHeaders" }, String("ParseHeaders"));

    if (auto parse_result = arg_parse.Parse(argc, argv)) {
        TaskSystem::GetInstance().Start();

        buildtool::HypBuildTool build_tool {
            FilePath(parse_result.result["WorkingDirectory"].AsString())
        };
        build_tool.Run();

        TaskSystem::GetInstance().Stop();
    } else {
        HYP_LOG(
            BuildTool,
            LogLevel::ERR,
            "Failed to parse arguments!\n\t{}",
            parse_result.message.HasValue()
                ? *parse_result.message
                : String("<no message>")
        );

        return 1;
    }

    return 0;
}
