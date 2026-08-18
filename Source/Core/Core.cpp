/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Core/Core.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Containers/List.hpp>

#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Reflection/TypeInfo.hpp>
#include <Core/Utilities/Uuid.hpp>

namespace Hyperion {

namespace threading {
extern void Task_DeleteAllDeferredTasks();
} // namespace threading

using threading::Task_DeleteAllDeferredTasks;

namespace CoreApi {

static Mutex s_globalsMutex;
static FilePath s_executablePath;
static Array<void (*)()> s_onShutdownFuncs;

extern "C"
{
    HYP_EXPORT void Hyp_TlsfAssert(int cond)
    {
        using namespace Hyperion;

        Assert(cond);
    }
}

CORE_API const FilePath& GetExecutablePath()
{
    return s_executablePath;
}

CORE_API void SetExecutablePath(const FilePath& path)
{
    // set once at the beginning of the application lifecycle,
    // so we don't guard it under a mutex

    s_executablePath = path;
}

CORE_API const FilePath& GetBaseDirectory()
{
    static struct BaseDirectoryInitializer
    {
        FilePath baseDir;

        BaseDirectoryInitializer()
        {
            const CommandLineArguments& cliArgs = GetCommandLineArguments();

#if !defined(HYP_ANDROID) && !defined(HYP_IOS)
            auto it = cliArgs.Find("BaseDir");
            if (it != cliArgs.End())
            {
                const auto& value = it->second;
                AssertDebug(value.IsString(), "Expected a string value for BaseDir");

                const String valueString = value.ToString();
                const bool isRelativePath = valueString.StartsWith(".");

                if (isRelativePath)
                {
                    Array<String> pathParts = GetExecutablePath().Split('\\', '/');
                    pathParts.Concat(valueString.Split('\\', '/'));

                    // canonicalize the path
                    pathParts = StringUtil::CanonicalizePath(pathParts);

#if HYP_WINDOWS
                    baseDir = String::Join(pathParts, '\\');
#else   // !HYP_WINDOWS
                    baseDir = String::Join(pathParts, '/');
#endif  // HYP_WINDOW
                }
                else
                {
                    baseDir = valueString;
                }

                return;
            }
#endif // !HYP_ANDROID

            baseDir = GetExecutablePath();
        }

    } s_initializer;

    return s_initializer.baseDir;
}

static List<GlobalConfig> s_globalConfigChain;
static Mutex s_globalConfigMutex;

static CommandLineArguments s_commandLineArguments;

CORE_API const CommandLineArgumentDefinitions& DefaultCommandLineArgumentDefinitions()
{
    static const struct DefaultCommandLineArgumentDefinitionsInitializer
    {
        CommandLineArgumentDefinitions definitions;

        DefaultCommandLineArgumentDefinitionsInitializer()
        {
            definitions.Add("Profile", {}, "Enable collection of profiling data for functions that opt in using HYP_SCOPE.", CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("TraceURL", {}, "The endpoint url that profiling data will be submitted to (this url will have /start appended to it to start the session and /results to add results)", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING);
            definitions.Add("BaseDir", {}, "Base directory", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING);
            definitions.Add("CacheDir", {}, "Directory for loading blob cache data (or saving for cook)", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING);
            definitions.Add("ContentDir", {}, "Directory for loading content manifest files", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING);
            definitions.Add("CacheServer", {}, "Endpoint to sync cache from", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING);
            definitions.Add("ResX", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::INTEGER);
            definitions.Add("ResY", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::INTEGER);
            definitions.Add("Headless", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("HighDPI", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("Detached", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("Editor", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("server", {}, "Launch standalone game as headless authoritative server", CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("host", {}, "Provide host address for connecting to a game server", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING, false);
            definitions.Add("exec", "", "Execute the commandlet with the given name immediately following --exec. The program will end immediately after running the commandlet and return 0 upon success or otherwise on failure", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING);

            definitions.Add("RenderOnMainThread",
                            {},
                            "Run rendering on the main thread instead of using dedicated render thread.",
                            CommandLineArgumentFlags::NONE,
                            CommandLineArgumentType::BOOLEAN,
                            false);

            definitions.Add("SimulateOnMainThread",
                            {},
                            "Simulate game logic on the main thread. Not compatible with -RenderOnMainThread.",
                            CommandLineArgumentFlags::NONE,
                            CommandLineArgumentType::BOOLEAN,
                            true);

            definitions.Add("DedicatedVisThread",
                            {},
                            "Use a dedicated thread for setting visibility states. If set to false, visibility will be computed on the simulation thread during normal frame processing.",
                            CommandLineArgumentFlags::NONE,
                            CommandLineArgumentType::BOOLEAN,
                            false);

            definitions.Add("Mode", "m", {}, CommandLineArgumentFlags::NONE, Array<String> { "precompile_shaders", "editor" }, String("editor"));
        }
    } s_initializer;

    return s_initializer.definitions;
}

CORE_API bool Initialize(int argc, char** argv)
{
    Assert(argv != nullptr);

    TypeInfo_Initialize();

    s_commandLineArguments = CommandLineArguments(argv[0]);

    CommandLineParser argParse { &DefaultCommandLineArgumentDefinitions() };

    TResult<CommandLineArguments> parseResult = argParse.Parse(argc, argv, false);

    if (parseResult.HasError())
    {
        const Error& error = parseResult.GetError();

        // Can't use Logger here, may not be init yet. So we just printf the error to stderr and exit.
        std::fprintf(stderr, "Error parsing command line arguments: %s\n", error.GetMessage());
        std::exit(1);

        return false;
    }

    s_commandLineArguments = CommandLineArguments::Merge(*argParse.GetDefinitions(), s_commandLineArguments, *parseResult);

    GlobalConfig config { "GlobalConfig" };
    config.Load();
    config.LogErrors(stderr);

    if (JSON::Value configArgs = config.Get("App.Args"))
    {
        JSON::JString configArgsString = configArgs.ToString();

        Array<String> configArgsStringSplit = MapToArray(
            configArgsString.Split(' '),
            [](auto&& str)
            {
                return str.ToUtf8();
            });

        parseResult = argParse.Parse(s_commandLineArguments.GetCommand(), configArgsStringSplit, false);

        if (!parseResult.HasError())
        {
            // merge argv last so that they may override what's in the config.
            s_commandLineArguments = CommandLineArguments::Merge(*argParse.GetDefinitions(), *parseResult, s_commandLineArguments);
        }
    }

    argParse.ApplyDefaults(s_commandLineArguments);

    return true;
}

CORE_API const CommandLineArguments& GetCommandLineArguments()
{
    return s_commandLineArguments;
}

CORE_API void UpdateGlobalConfig(const ConfigBase& mergeValues)
{
    Mutex::Guard guard(s_globalConfigMutex);

    GlobalConfig* prevGlobalConfig = nullptr;

    if (s_globalConfigChain.Any())
    {
        prevGlobalConfig = &s_globalConfigChain.Back();
    }

    GlobalConfig& newGlobalConfig = s_globalConfigChain.EmplaceBack("GlobalConfig");

    if (prevGlobalConfig != nullptr)
    {
        newGlobalConfig.Merge(*prevGlobalConfig);
    }

    newGlobalConfig.Merge(mergeValues);
    newGlobalConfig.Save();

    for (GlobalConfig& curr : s_globalConfigChain)
    {
        curr.SetNewRevision(&newGlobalConfig);
    }
}

CORE_API const GlobalConfig& GetGlobalConfig()
{
    Mutex::Guard guard(s_globalConfigMutex);

    if (s_globalConfigChain.Empty())
    {
        GlobalConfig& cfg = s_globalConfigChain.EmplaceBack("GlobalConfig");
        cfg.Load();
    }

    return s_globalConfigChain.Back();
}

#if HYP_ENABLE_PROFILE
CORE_API bool IsProfilingEnabled()
{
    // only check once since it won't change and we call from some hot paths
    static const bool s_isProfilingEnabled = GetCommandLineArguments()["Profile"].ToBool();

    return s_isProfilingEnabled;
}
#endif

CORE_API void OnShutdown(void (*func)())
{
    Mutex::Guard guard(s_globalsMutex);
    s_onShutdownFuncs.PushBack(func);
}

CORE_API void Shutdown()
{
    TypeInfo_Shutdown();
    Task_DeleteAllDeferredTasks();

    {
        Mutex::Guard guard(s_globalsMutex);

        for (size_t i = s_onShutdownFuncs.Size(); i > 0; --i)
        {
            s_onShutdownFuncs[i - 1]();
        }

        s_onShutdownFuncs.Clear();
    }
}

} // namespace CoreApi
} // namespace Hyperion
