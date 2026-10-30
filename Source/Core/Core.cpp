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

#include <Core/IO/ByteReader.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(Core);

namespace threading {
extern void Task_DeleteAllDeferredTasks();
} // namespace threading

using threading::Task_DeleteAllDeferredTasks;

namespace CoreApi {

namespace {

Mutex g_globalsMutex;
FilePath g_executablePath;
Array<void (*)()> g_onShutdownFuncs;

List<GlobalConfig> g_globalConfigChain;
Mutex g_globalConfigMutex;

CommandLineArguments g_commandLineArguments;
Array<String> g_commandLineArgumentStrings;

CommandLineArgumentRegistration g_argBaseDir {
    "basedir",
    {},
    "Base directory",
    CommandLineArgumentFlags::NONE,
    CommandLineArgumentType::STRING
};

CommandLineArgumentRegistration g_argConfigVariant {
    "configvariant",
    {},
    "Read and save Config/<Name>.<variant>.json in place of Config/<Name>.json, where there is one",
    CommandLineArgumentFlags::NONE,
    CommandLineArgumentType::STRING
};

} // namespace

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
    return g_executablePath;
}

CORE_API void SetExecutablePath(const FilePath& path)
{
    // set once at the beginning of the application lifecycle,
    // so we don't guard it under a mutex

    g_executablePath = path;
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
            auto it = cliArgs.Find("basedir");
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

CORE_API const CommandLineArgumentDefinitions& DefaultCommandLineArgumentDefinitions()
{
    static const CommandLineArgumentDefinitions definitions(GetRegisteredCommandLineArgumentDefinitions());

    return definitions;
}

CORE_API bool Initialize(int argc, char** argv)
{
    Assert(argv != nullptr);

    TypeInfo_Initialize();

    g_commandLineArguments = CommandLineArguments(argv[0]);

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

    g_commandLineArguments = CommandLineArguments::Merge(*argParse.GetDefinitions(), g_commandLineArguments, *parseResult);

    GlobalConfig config { "GlobalConfig" };
    config.Load();
    config.LogErrors(stderr);

    CommandLineArguments configuredArgs(g_commandLineArguments.GetCommand());

    if (JSON::Value configArgs = config.Get("App.Args"))
    {
        JSON::JString configArgsString = configArgs.ToString();

        const Array<String> configArgStrings = CommandLineParser::SplitArguments(configArgsString.ToUtf8());

        parseResult = argParse.Parse(g_commandLineArguments.GetCommand(), configArgStrings, false);

        if (!parseResult.HasError())
        {
            configuredArgs = *parseResult;

            g_commandLineArgumentStrings.Concat(configArgStrings);
        }
    }

    const FilePath argsFilePath = GetExecutablePath() / "args.txt";

    if (argsFilePath.Exists())
    {
        FileByteReader argsFileReader { argsFilePath };

        const String argsFileText = String(argsFileReader.Read().ToByteView())
            .ReplaceAll("\r", " ")
            .ReplaceAll("\n", " ");

        argsFileReader.Close();

        const Array<String> argsFileStrings = CommandLineParser::SplitArguments(argsFileText);

        parseResult = argParse.Parse(
            g_commandLineArguments.GetCommand(),
            argsFileStrings,
            false);

        if (parseResult.HasError())
        {
            std::fprintf(stderr, "Error parsing %s: %s\n", argsFilePath.Data(), parseResult.GetError().GetMessage());
        }
        else
        {
            configuredArgs = CommandLineArguments::Merge(*argParse.GetDefinitions(), configuredArgs, *parseResult);

            g_commandLineArgumentStrings.Concat(argsFileStrings);
        }
    }

    for (int i = 1; i < argc; i++)
    {
        g_commandLineArgumentStrings.PushBack(argv[i]);
    }

    // merge argv last so that they may override what's in the config and args.txt.
    g_commandLineArguments = CommandLineArguments::Merge(*argParse.GetDefinitions(), configuredArgs, g_commandLineArguments);

    argParse.ApplyDefaults(g_commandLineArguments);

    return true;
}

CORE_API const CommandLineArguments& GetCommandLineArguments()
{
    return g_commandLineArguments;
}

CORE_API CommandLineArguments ParseCommandLineArguments()
{
    const CommandLineArgumentDefinitions definitions(GetRegisteredCommandLineArgumentDefinitions());

    CommandLineParser argParse { &definitions };

    TResult<CommandLineArguments> parseResult = argParse.Parse(g_commandLineArguments.GetCommand(), g_commandLineArgumentStrings);

    if (parseResult.HasError())
    {
        HYP_LOG(Core, Error, "Error parsing command line arguments: {}", parseResult.GetError().GetMessage());

        return g_commandLineArguments;
    }

    return *parseResult;
}

CORE_API void UpdateGlobalConfig(const ConfigBase& mergeValues)
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

    for (GlobalConfig& curr : g_globalConfigChain)
    {
        curr.SetNewRevision(&newGlobalConfig);
    }
}

CORE_API const GlobalConfig& GetGlobalConfig()
{
    Mutex::Guard guard(g_globalConfigMutex);

    if (g_globalConfigChain.Empty())
    {
        GlobalConfig& cfg = g_globalConfigChain.EmplaceBack("GlobalConfig");
        cfg.Load();
    }

    return g_globalConfigChain.Back();
}

CORE_API void OnShutdown(void (*func)())
{
    Mutex::Guard guard(g_globalsMutex);
    g_onShutdownFuncs.PushBack(func);
}

CORE_API void Shutdown()
{
    TypeInfo_Shutdown();
    Task_DeleteAllDeferredTasks();

    Mutex::Guard guard(g_globalsMutex);

    for (size_t i = g_onShutdownFuncs.Size(); i > 0; --i)
    {
        g_onShutdownFuncs[i - 1]();
    }

    g_onShutdownFuncs.Clear();
}

} // namespace CoreApi
} // namespace Hyperion
