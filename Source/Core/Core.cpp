/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/Core.hpp>

#include <Core/threading/Mutex.hpp>

#include <Core/containers/LinkedList.hpp>

#include <Core/reflection/BoxedValue.hpp>

#include <Core/reflection/TypeInfo.hpp>
#include <Core/utilities/Uuid.hpp>

namespace Hyperion {

namespace threading {
HYP_API extern void Task_DeleteAllDeferredTasks();
} // namespace threading

using threading::Task_DeleteAllDeferredTasks;

namespace CoreApi {

static Mutex s_globalsMutex;
static FilePath s_executablePath;
static FilePath s_configDirectory;
static Array<void (*)()> s_onShutdownFuncs;

FilePath GetExecutablePath()
{
    Mutex::Guard guard(s_globalsMutex);
    return s_executablePath;
}

void SetExecutablePath(const FilePath& path)
{
    Mutex::Guard guard(s_globalsMutex);
    s_executablePath = path;
}


FilePath GetConfigDirectory()
{
    Mutex::Guard guard(s_globalsMutex);
    return s_configDirectory;
}

void SetConfigDirectory(const FilePath& configDirectory)
{
    Mutex::Guard guard(s_globalsMutex);
    s_configDirectory = configDirectory;
}

HYP_NODISCARD FilePath CreateTempDirectory()
{
    Mutex::Guard guard(s_globalsMutex);

    FilePath basePath = s_executablePath / "Temp";

    if (basePath.Empty())
    {
        return FilePath();
    }

    if (!basePath.Exists() && !basePath.MkDir())
    {
        return FilePath();
    }

    for (uint32 attempt = 0; attempt < 16; ++attempt)
    {
        const String uuidString = UUID().ToString().ReplaceAll("-", "");
        const String randomSuffix = String(uuidString.Substr(0, 6));
        const FilePath tempPath = basePath / randomSuffix;

        if (tempPath.MkDir())
        {
            return tempPath;
        }
    }

    return FilePath();
}

static LinkedList<GlobalConfig> s_globalConfigChain;
static Mutex s_globalConfigMutex;

static CommandLineArguments s_commandLineArguments;

const CommandLineArgumentDefinitions& DefaultCommandLineArgumentDefinitions()
{
    static const struct DefaultCommandLineArgumentDefinitionsInitializer
    {
        CommandLineArgumentDefinitions definitions;

        DefaultCommandLineArgumentDefinitionsInitializer()
        {
            definitions.Add("Profile", {}, "Enable collection of profiling data for functions that opt in using HYP_SCOPE.", CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("TraceURL", {}, "The endpoint url that profiling data will be submitted to (this url will have /start appended to it to start the session and /results to add results)", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING);
            definitions.Add("ResX", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::INTEGER);
            definitions.Add("ResY", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::INTEGER);
            definitions.Add("Headless", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("Detached", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("Editor", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("Commandlet", "c", "Execute the commandlet with the given name immediately following -Commandlet. The program will end immediately after running the commandlet and return 0 upon success or otherwise on failure", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING);

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
    } initializer;

    return initializer.definitions;
}

bool Initialize(int argc, char** argv)
{
    Assert(argv != nullptr);

    TypeInfo_Initialize();

    s_commandLineArguments = CommandLineArguments(argv[0]);

    CommandLineParser argParse { &DefaultCommandLineArgumentDefinitions() };

    TResult<CommandLineArguments> parseResult = argParse.Parse(argc, argv);

    if (parseResult.HasError())
    {
        const Error& error = parseResult.GetError();

        return false;
    }

    s_commandLineArguments = CommandLineArguments::Merge(*argParse.GetDefinitions(), s_commandLineArguments, *parseResult);

    GlobalConfig config { "GlobalConfig" };

    if (JSON::Value configArgs = config.Get("App.Args"))
    {
        JSON::JString configArgsString = configArgs.ToString();
        Array<String> configArgsStringSplit = Map(
            configArgsString.Split(' '),
            [](auto&& str)
            {
                return str.ToUtf8();
            });

        parseResult = argParse.Parse(s_commandLineArguments.GetCommand(), configArgsStringSplit);

        if (!parseResult.HasError())
        {
            // merge argv last so that they may be override what's in the config.
            s_commandLineArguments = CommandLineArguments::Merge(*argParse.GetDefinitions(), *parseResult, s_commandLineArguments);
        }
    }

    return true;
}

const CommandLineArguments& GetCommandLineArguments()
{
    return s_commandLineArguments;
}

void UpdateGlobalConfig(const ConfigurationTable& mergeValues)
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
}

const GlobalConfig& GetGlobalConfig()
{
    Mutex::Guard guard(s_globalConfigMutex);

    if (s_globalConfigChain.Empty())
    {
        s_globalConfigChain.EmplaceBack("GlobalConfig");
    }

    return s_globalConfigChain.Back();
}

void OnShutdown(void (*func)())
{
    Mutex::Guard guard(s_globalsMutex);
    s_onShutdownFuncs.PushBack(func);
}

void Shutdown()
{
    TypeInfo_Shutdown();
    Task_DeleteAllDeferredTasks();

    {
        Mutex::Guard guard(s_globalsMutex);

        for (SizeType i = s_onShutdownFuncs.Size(); i > 0; --i)
        {
            s_onShutdownFuncs[i - 1]();
        }

        s_onShutdownFuncs.Clear();
    }
}

} // namespace CoreApi
} // namespace Hyperion
