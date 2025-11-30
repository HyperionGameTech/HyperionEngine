/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/Core.hpp>

#include <core/threading/Mutex.hpp>

#include <core/containers/LinkedList.hpp>

#include <core/reflection/HypData.hpp>

#include <core/reflection/TypeInfo.hpp>

namespace hyperion {

namespace threading {
HYP_API extern void Task_DeleteAllDeferredTasks();
} // namespace threading

using threading::Task_DeleteAllDeferredTasks;

static Mutex s_globalsMutex;
static FilePath s_executablePath;
static Array<void (*)()> s_onShutdownFuncs;

FilePath CoreApi_GetExecutablePath()
{
    Mutex::Guard guard(s_globalsMutex);
    return s_executablePath;
}

void CoreApi_SetExecutablePath(const FilePath& path)
{
    Mutex::Guard guard(s_globalsMutex);
    s_executablePath = path;
}

static LinkedList<GlobalConfig> s_globalConfigChain;
static Mutex s_globalConfigMutex;

static CommandLineArguments s_commandLineArguments;

const CommandLineArgumentDefinitions& CoreApi_DefaultCommandLineArgumentDefinitions()
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
            definitions.Add("RenderOnMainThread", {}, "Run rendering on the main thread instead of using dedicated render thread.", CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("Mode", "m", {}, CommandLineArgumentFlags::NONE, Array<String> { "precompile_shaders", "editor" }, String("editor"));
        }
    } initializer;

    return initializer.definitions;
}

bool CoreApi_Initialize(int argc, char** argv)
{
    Assert(argv != nullptr);

    TypeInfo_Initialize();

    s_commandLineArguments = CommandLineArguments(argv[0]);

    CommandLineParser argParse { &CoreApi_DefaultCommandLineArgumentDefinitions() };

    TResult<CommandLineArguments> parseResult = argParse.Parse(argc, argv);

    if (parseResult.HasError())
    {
        const Error& error = parseResult.GetError();

        return false;
    }

    s_commandLineArguments = CommandLineArguments::Merge(*argParse.GetDefinitions(), s_commandLineArguments, *parseResult);

    GlobalConfig config { "GlobalConfig" };

    if (json::JSONValue configArgs = config.Get("App.Args"))
    {
        json::JSONString configArgsString = configArgs.ToString();
        Array<String> configArgsStringSplit = configArgsString.Split(' ');

        parseResult = argParse.Parse(s_commandLineArguments.GetCommand(), configArgsStringSplit);

        if (!parseResult.HasError())
        {
            // merge argv last so that they may be override what's in the config.
            s_commandLineArguments = CommandLineArguments::Merge(*argParse.GetDefinitions(), *parseResult, s_commandLineArguments);
        }
    }

    return true;
}

const CommandLineArguments& CoreApi_GetCommandLineArguments()
{
    return s_commandLineArguments;
}

void CoreApi_UpdateGlobalConfig(const ConfigurationTable& mergeValues)
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

const GlobalConfig& CoreApi_GetGlobalConfig()
{
    Mutex::Guard guard(s_globalConfigMutex);

    if (s_globalConfigChain.Empty())
    {
        s_globalConfigChain.EmplaceBack("GlobalConfig");
    }

    return s_globalConfigChain.Back();
}

void CoreApi_OnShutdown(void (*func)())
{
    Mutex::Guard guard(s_globalsMutex);
    s_onShutdownFuncs.PushBack(func);
}

void CoreApi_Shutdown()
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

} // namespace hyperion