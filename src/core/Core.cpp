/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/Core.hpp>
#include <core/threading/Mutex.hpp>
#include <core/containers/LinkedList.hpp>
#include <core/utilities/TypeInfo.hpp>

namespace hyperion {

static Mutex g_globalsMutex;
static FilePath g_executablePath;
static Array<void (*)()> g_onShutdownFuncs;

FilePath CoreApi_GetExecutablePath()
{
    Mutex::Guard guard(g_globalsMutex);
    return g_executablePath;
}

void CoreApi_SetExecutablePath(const FilePath& path)
{
    Mutex::Guard guard(g_globalsMutex);
    g_executablePath = path;
}

static LinkedList<GlobalConfig> g_globalConfigChain;
static Mutex g_globalConfigMutex;

static CommandLineArguments g_commandLineArguments;

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
            definitions.Add("Mode", "m", {}, CommandLineArgumentFlags::NONE, Array<String> { "precompile_shaders", "editor" }, String("editor"));
        }
    } initializer;

    return initializer.definitions;
}

bool CoreApi_Initialize(int argc, char** argv)
{
    g_commandLineArguments = CommandLineArguments(argv[0]);

    CommandLineParser argParse { &CoreApi_DefaultCommandLineArgumentDefinitions() };

    TResult<CommandLineArguments> parseResult = argParse.Parse(argc, argv);

    if (parseResult.HasError())
    {
        const Error& error = parseResult.GetError();

        return false;
    }

    GlobalConfig config { "GlobalConfig" };

    if (json::JSONValue configArgs = config.Get("app.args"))
    {
        json::JSONString configArgsString = configArgs.ToString();
        Array<String> configArgsStringSplit = configArgsString.Split(' ');

        parseResult = argParse.Parse(g_commandLineArguments.GetCommand(), configArgsStringSplit);

        if (parseResult.HasError())
        {
            return false;
        }

        g_commandLineArguments = CommandLineArguments::Merge(*argParse.GetDefinitions(), *parseResult, g_commandLineArguments);
    }

    return true;
}

const CommandLineArguments& CoreApi_GetCommandLineArguments()
{
    return g_commandLineArguments;
}

void CoreApi_UpdateGlobalConfig(const ConfigurationTable& mergeValues)
{
    Mutex::Guard guard(g_globalConfigMutex);

    GlobalConfig* prevGlobalConfig = nullptr;

    if (g_globalConfigChain.Any())
    {
        prevGlobalConfig = &g_globalConfigChain.Back();
    }

    GlobalConfig& newGlobalConfig = g_globalConfigChain.EmplaceBack("GlobalConfig");

    if (prevGlobalConfig != nullptr)
    {
        newGlobalConfig.Merge(*prevGlobalConfig);
    }

    newGlobalConfig.Merge(mergeValues);
    newGlobalConfig.Save();
}

const GlobalConfig& CoreApi_GetGlobalConfig()
{
    Mutex::Guard guard(g_globalConfigMutex);

    if (g_globalConfigChain.Empty())
    {
        g_globalConfigChain.EmplaceBack("GlobalConfig");
    }

    return g_globalConfigChain.Back();
}

void CoreApi_OnShutdown(void (*func)())
{
    Mutex::Guard guard(g_globalsMutex);
    g_onShutdownFuncs.PushBack(func);
}

void CoreApi_Shutdown()
{
    TypeInfo_DestroyCache();

    {
        Mutex::Guard guard(g_globalsMutex);

        for (SizeType i = g_onShutdownFuncs.Size(); i > 0; --i)
        {
            g_onShutdownFuncs[i - 1]();
        }

        g_onShutdownFuncs.Clear();
    }
}

} // namespace hyperion