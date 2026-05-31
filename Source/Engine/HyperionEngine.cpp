/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <HyperionEngine.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineStats.hpp>
#include <Framework/Game.hpp>
#include <Framework/CVarManager.hpp>

#include <Framework/Threads/MainThread.hpp>
#include <Framework/Threads/SimThread.hpp>
#include <Framework/Threads/RenderThread.hpp>
#include <Framework/Threads/VisThread.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/BlobStorage.hpp>

#include <Core/Core.hpp>

#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/TypeInfo.hpp>
#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/Method.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/TaskSystem.hpp>

#include <Core/Memory/Allocator/ArenaAllocator.hpp>
#include <Core/Memory/Pool/Pool.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <System/MessageBox.hpp>
#include <System/AppContext.hpp>
#include <System/DirectoryInitializer.hpp>

#include <Input/Event.hpp>

#include <Streaming/StreamingManager.hpp>

#include <Rendering/MaterialDefinition.hpp>
#include <Rendering/MaterialInstance.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/DebugDrawer.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderCompiler.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/ComponentInterface.hpp>

#include <UI/UIDataSource.hpp> // For UIElementFactoryRegistry

#include <Audio/AudioManager.hpp>

#if HYP_VULKAN
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#endif // HYP_VULKAN

#if HYP_EDITOR
#include <Editor/EditorState.hpp>
#include <Editor/EditorCommand.hpp>
#include <Editor/EditorSubsystem.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#endif // HYP_EDITOR

#if HYP_DOTNET
#include <DotNET/DotNETHost.hpp>
#endif // HYP_DOTNET

#if HYP_ANDROID
#include <android/asset_manager.h>
#endif // HYP_ANDROID

/// ========== If this include is missing, you need to run the CodeGen tool (instructions in doc/CompilingTheEngine.md) ==========
#include <CodeGenOutput.inc>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Engine);

#if HYP_ANDROID
CORE_API extern AAssetManager* g_androidAssetManager;
#endif // HYP_ANDROID

#pragma region Memory Pools

#define HYP_ENGINE_MEMORY_IMPLEMENTATION 1
#include <Framework/EngineMemory.inc>
#undef HYP_ENGINE_MEMORY_IMPLEMENTATION

#pragma endregion Memory Pools

ENGINE_API extern void InitializeModule_Engine();
CORE_API extern void InitializeModule_Core();

#if HYP_EDITOR
EDITOR_API extern void InitializeModule_Editor();
#endif

// defined in PlatformUtils.[cpp|mm]
namespace PlatformUtils {
ENGINE_API extern PlatformString GetExecutableAbsolutePath();
} // namespace PlatformUtils

ENGINE_API Handle<EngineDriver> g_engineDriver;
ENGINE_API Handle<AssetManager> g_assetManager;
ENGINE_API Handle<AudioManager> g_audioManager;
ENGINE_API Handle<AppContextBase> g_appContext;
ENGINE_API Handle<StreamingManager> g_streamingManager;
ENGINE_API Handle<EngineStats> g_engineStats;
ENGINE_API MaterialInstanceCache* g_materialInstanceCache;
ENGINE_API ShaderCompiler* g_shaderCompiler;

#if HYP_EDITOR
Handle<EditorState> g_editorState;
#endif // HYP_EDITOR

MainThread* g_mainThreadInstance;
SimThread* g_simThreadInstance;
RenderThread* g_renderThreadInstance;
VisThread* g_visThreadInstance;

Game* g_gameInstance; // active game instance, read/write only from the main thread

#if HYP_VULKAN
VulkanRenderInterface RI;
#elif HYP_DX12
DX12RenderInterface RI;
#endif // HYP_VULKAN || HYP_DX12

static void HandleFatalError(const char* message)
{
    SystemMessageBox(MessageBoxType::CRITICAL)
        .Title("Fatal error logged!")
        .Text(message)
        .Show();

    debug::TerminateProgram();
}

HYP_EXPORT const FilePath& GetLibraryDirectory()
{
#if HYP_EDITOR
    static DirectoryInitializer<HYP_STATIC_STRING("Packages"), /* RelativeToExecutablePath */ false> s_resourceDirectory;
    return s_resourceDirectory.path;
#else // !HYP_EDITOR
    // shouldn't be used in non-editor builds, so just return executable path to avoid issues with appending paths
    HYP_LOG(Engine, Warning, "GetLibraryDirectory() called in non-editor build; returning executable path instead");

    static const FilePath s_exePath = CoreApi::GetExecutablePath();
    Assert(s_exePath.Length() != 0); // don't want to return empty path which will cause appending to give root-level paths.

    return s_exePath;
#endif // HYP_EDITOR
}

#if HYP_EDITOR
HYP_EXPORT const FilePath& GetProjectsDirectory()
{
    // @TODO Use configuration value for this path. can be in Documents folder eg

    static DirectoryInitializer<HYP_STATIC_STRING("Projects"), /* RelativeToExecutablePath */ false> s_projectsDirectory;
    return s_projectsDirectory.path;
}
#endif // HYP_EDITOR

// Directory for cached data (shader bundles, compiled scripts, etc.) Expected to be compiled into the asset registry in production builds
static bool s_cacheDirectoryInit = false;
static SharedMutex s_cacheDirectoryMutex;

HYP_EXPORT const FilePath& GetCacheDirectory()
{
    static const ConfigValue& s_cfgCacheDirectory = CoreApi::GetGlobalConfig().Get("App.Cache.BaseDirectory");
    static const ConfigValue& s_cfgCachePageSize = CoreApi::GetGlobalConfig().Get("App.Cache.PageSize");

    static const FilePath s_cacheDirectory = CoreApi::GetExecutablePath() / s_cfgCacheDirectory.ToString().ToUtf8();

    TSharedLock sharedLock(s_cacheDirectoryMutex);

    if (s_cacheDirectoryInit)
        return s_cacheDirectory;

    sharedLock.Reset();

    TUniqueLock uniqueLock(s_cacheDirectoryMutex);

    if (s_cacheDirectoryInit)
        return s_cacheDirectory;

    if (!s_cfgCachePageSize.IsNumber() || s_cfgCachePageSize.AsNumber() < 1024 * 1024)
    {
        ConfigBase newConfigurationTable;
        newConfigurationTable.Set("App.Cache.PageSize", ConfigValue(BlobStorage::DefaultPageSize));

        CoreApi::UpdateGlobalConfig(newConfigurationTable);
    }

    if (s_cacheDirectory.Empty() || (!s_cacheDirectory.Exists() && !s_cacheDirectory.MkDir()))
    {
        HYP_FAIL("Failed to initialize cache storage directory {}!", s_cacheDirectory);
    }

    s_cacheDirectoryInit = true;

    return s_cacheDirectory;
}

// Editor build only
HYP_EXPORT const FilePath& GetTempDirectory()
{
#if HYP_ANDROID
    // not used in Android build.
    static const FilePath s_emptyPath;
    return s_emptyPath;
#else // !HYP_ANDROID
    static DirectoryInitializer<HYP_STATIC_STRING("Temp"), /* RelativeToExecutablePath */ true> s_tempDirectory;
    return s_tempDirectory.path;
#endif // HYP_ANDROID
}

// Editor build only
HYP_EXPORT const FilePath& GetDataDirectory()
{
    static DirectoryInitializer<HYP_STATIC_STRING("Data"), /* RelativeToExecutablePath */ false> s_dataDirectory;
    return s_dataDirectory.path;
}

HYP_EXPORT const FilePath& GetConfigDirectory()
{
    static DirectoryInitializer<HYP_STATIC_STRING("Config"), /* RelativeToExecutablePath */ false> s_configDirectory;
    return s_configDirectory.path;
}

#if HYP_DOTNET
static InitFromManagedCallback s_initFromManagedCallback = nullptr;
#endif

static void InitThreads()
{
    // Handle -RenderOnMainThread, -SimulateOnMainThread cli args
    const uint32 mainThreadIndex = g_mainThread.GetStaticThreadIndex();

    if (CoreApi::GetCommandLineArguments()["RenderOnMainThread"].ToBool())
    {
        g_renderThread = StaticThreadId(mainThreadIndex, NAME("Render"));
        g_simThread = StaticThreadId(NAME("Simulation"));
    }
    else
    {
        g_renderThread = StaticThreadId(NAME("Render"));

        if (CoreApi::GetCommandLineArguments()["SimulateOnMainThread"].ToBool())
        {
            g_simThread = StaticThreadId(mainThreadIndex, NAME("Simulation"));
        }
        else
        {
            g_simThread = StaticThreadId(NAME("Simulation"));
        }
    }

    if (CoreApi::GetCommandLineArguments()["DedicatedVisThread"].ToBool())
    {
        g_visThread = StaticThreadId(NAME("Visibility"));
    }
    else
    {
        // use sim thread for visibility state updates
        g_visThread = g_simThread;
    }

    g_mainThreadInstance = new MainThread();
    SetCurrentThreadObject(g_mainThreadInstance);

    g_renderThreadInstance = new RenderThread();
    g_simThreadInstance = new SimThread();
    g_visThreadInstance = new VisThread();
}

static void InitLogger()
{
    Logger::GetInstance().fatalErrorHook = &HandleFatalError;

    LogChannelRegistrar::GetInstance().RegisterAll();
}

static void LoadShaderPropertyDictionary()
{
    InitShaderPropertyDictionary();

    FileByteReader stream { GetCacheDirectory() / "ShaderProperties.bin" };

    if (!stream.Eof())
    {
        ReadShaderPropertyDictionary(stream);
    }
}

extern "C"
{
    HYP_EXPORT int Hyp_Initialize(int argc, char** argv)
    {
        if (!argv || argc <= 0)
        {
            // we need argc/argv to be passed by the caller
            return 0;
        }

        SetCurrentThreadId(g_mainThread);

        InitializeModule_Core();
        InitializeModule_Engine();
#if HYP_EDITOR
        InitializeModule_Editor();
#endif

        CoreApi::SetConfigDirectory(GetConfigDirectory());

        if (!CoreApi::Initialize(argc, argv))
        {
            return 0;
        }

        EngineConfig engineConfig;
        engineConfig.Load();

        CVarManager::GetInstance().InitFromConfig(engineConfig);

        InitThreads();
        InitMemoryPools();
        InitNameRegistry();

        ClassRegistry::GetInstance().Initialize();

        InitLogger();

        const CommandLineArguments& cliArgs = CoreApi::GetCommandLineArguments();

#if HYP_ANDROID
        // use asset manager for all assets
        const FilePath basePath = FilePath(AndroidAssetPathPrefix);
#else // !HYP_ANDROID
        const FilePath basePath = FilePath(PlatformUtils::GetExecutableAbsolutePath().ToUtf8()).BasePath();
#endif // HYP_ANDROID

        CoreApi::SetExecutablePath(basePath);

        const bool isEditor = cliArgs["Editor"].ToBool();
        const bool isCommandlet = cliArgs["exec"].ToBool();

#if HYP_DOTNET && !defined(HYP_COMMANDLET_NAME)
        if (!isCommandlet)
        {
            DotNETHost::GetInstance().Initialize(basePath, /* initFromManaged */ isEditor, s_initFromManagedCallback);
        }
#endif // HYP_DOTNET

        g_engineDriver = MakeHandle<EngineDriver>();

        ComponentInterfaceRegistry::GetInstance().Initialize();

        g_engineStats = MakeHandle<EngineStats>();

        g_streamingManager = MakeHandle<StreamingManager>();
        g_streamingManager->Start();

        g_assetManager = MakeHandle<AssetManager>();
        g_assetManager->Initialize();

        // Create the engine-global asset registry for shared engine data (shaders, debug shapes, etc.)
        {
            Handle<AssetRegistry> engineRegistry = MakeHandle<AssetRegistry>(
                AssetRegistryId::Engine,
                GetLibraryDirectory() / "Engine");

            engineRegistry->Initialize();

            SetEngineAssetRegistry(engineRegistry);
        }

#if HYP_EDITOR
        // Create the editor asset registry
        {
            Handle<AssetRegistry> editorRegistry = MakeHandle<AssetRegistry>(
                AssetRegistryId::Editor,
                GetLibraryDirectory() / "Editor");

            editorRegistry->Initialize();

            SetEditorAssetRegistry(editorRegistry);
        }
#endif // HYP_EDITOR

        g_audioManager = MakeHandle<AudioManager>();
        g_audioManager->Initialize();

#if HYP_EDITOR
        g_editorState = MakeHandle<EditorState>();
        g_editorState->Initialize();
#endif // HYP_EDITOR

        g_materialInstanceCache = new MaterialInstanceCache;

        LoadShaderPropertyDictionary();

        g_shaderCompiler = new ShaderCompiler;
        if (!g_shaderCompiler->LoadShaderDefinitions())
        {
            HYP_LOG(Engine, Error, "Failed to load shader definitions!");
        }

#if HYP_WINDOWS
        g_appContext = MakeHandle<Win32AppContext>("Hyperion", cliArgs);
#elif HYP_MACOS
        g_appContext = MakeHandle<CocoaAppContext>("Hyperion", cliArgs);
#elif HYP_SDL
        g_appContext = MakeHandle<SDLAppContext>("Hyperion", cliArgs);
#elif HYP_ANDROID
        g_appContext = MakeHandle<AndroidAppContext>("Hyperion", cliArgs);
#else // !HYP_WINDOWS && !HYP_MACOS && !HYP_SDL && !HYP_ANDROID
        HYP_FAIL("AppContext not implemented for this platform");
#endif // HYP_WINDOWS || HYP_MACOS || HYP_SDL || HYP_ANDROID

        g_engineDriver->Initialize();

        if (isCommandlet)
        {
            const ANSIString commandletName = cliArgs["exec"].ToString().ToAnsi();

            const Class* commandletClass = g_appContext->FindCommandletClass(commandletName);

            if (!commandletClass)
            {
                HYP_LOG(Engine, Error, "Failed to find Commandlet class with name: {}", commandletName);

                Hyp_Shutdown();

                return 1;
            }

            String cliString;

            for (int i = 1; i < argc; i++)
            {
                cliString += argv[i];
                if (i != argc - 1)
                {
                    cliString += ' ';
                }
            }

            CommandLineArgumentDefinitions argumentDefinitions {};

            // check for static method GetArgumentDefinitions() on commandlet class to override.
            if (const Method* m = commandletClass->GetMethod("GetArgumentDefinitions"_sh))
            {
                Span<BoxedValue*> args = { nullptr };

                BoxedValue boxed = m->Invoke(args);
                AssertDebug(boxed.Is<CommandLineArgumentDefinitions>());

                if (boxed.Is<CommandLineArgumentDefinitions>())
                {
                    argumentDefinitions = boxed.Get<CommandLineArgumentDefinitions>();
                }
            }

            CommandLineParser parser { &argumentDefinitions };
            TResult<CommandLineArguments> parseResult = parser.Parse(cliString);

            if (parseResult.HasError())
            {
                HYP_LOG(Engine, Error, "Failed to parse command line arguments: {}", parseResult.GetError().GetMessage());
                return 1;
            }

            CommandLineArguments& commandletArgs = parseResult.GetValue();
            commandletArgs.Delete("exec");

            Result commandletResult = g_appContext->RunCommandlet(commandletName, commandletArgs);

            if (commandletResult.HasError())
            {
                HYP_LOG(Engine, Error, "Commandlet execution failed! {}", commandletResult.GetError().GetMessage());
            }

            ThreadSleep(1000);

            Hyp_Shutdown();

            std::exit(commandletResult.HasError() ? 1 : 0);

            return 0;
        }

        EnumFlags<WindowFlags> windowFlags = WindowFlags::EVENTS_POLLING;

        if (cliArgs["Headless"].ToBool())   windowFlags |= WindowFlags::HEADLESS;
        if (cliArgs["HighDPI"].ToBool())    windowFlags |= WindowFlags::HIGH_DPI;

        if (!(windowFlags & WindowFlags::HEADLESS))
        {
            Vec2i resolution = { 1280, 720 };

            if (cliArgs["ResX"].IsNumber()) resolution.x = cliArgs["ResX"].ToInt32();
            if (cliArgs["ResY"].IsNumber()) resolution.y = cliArgs["ResY"].ToInt32();

            HYP_LOG(Engine, Info, "Running in windowed mode: {}x{}", resolution.x, resolution.y);

            Handle<ApplicationWindow> window = g_appContext->CreateSystemWindow({ "Hyperion Engine", resolution, windowFlags });

            window->OnClose
                .Bind([]()
                {
                    // shut down application on main window close.
                    Hyp_Shutdown();

                    std::exit(0);
                })
                .Detach();

            Assert(window.IsValid());

            g_appContext->SetMainWindow(window);
        }
        else
        {
            HYP_LOG(Engine, Info, "Running in headless mode");
        }

        return 1;
    }

    HYP_EXPORT void Hyp_Shutdown()
    {
        AssertOnThread(g_mainThread);

        Assert(g_engineDriver != nullptr, "Hyperion not initialized!");

        g_engineDriver->RequestStop();

#if HYP_DOTNET
        DotNETHost::GetInstance().Shutdown();
#endif // HYP_DOTNET

        g_mainThreadInstance->Stop();

        g_renderThreadInstance->Join();
        g_renderThread = g_mainThread;

        g_simThreadInstance->Join();
        g_simThread = g_mainThread;

        g_engineDriver->FinalizeStop();

        g_streamingManager->Stop();
        g_streamingManager.Reset();

        g_audioManager->Shutdown();
        g_audioManager.Reset();

        ClearAssetRegistryStack();

        GetEngineAssetRegistry()->Shutdown();
        SetEngineAssetRegistry(Handle<AssetRegistry>::Null());

#if HYP_EDITOR
        GetEditorAssetRegistry()->Shutdown();
        SetEditorAssetRegistry(Handle<AssetRegistry>::Null());
#endif // HYP_EDITOR

        g_assetManager.Reset();
        g_engineDriver.Reset();
        g_engineStats.Reset();

#if HYP_EDITOR
        g_editorState.Reset();
#endif // HYP_EDITOR

        g_appContext.Reset();

        ComponentInterfaceRegistry::GetInstance().Shutdown();

        UIElementFactoryRegistry::GetInstance().Shutdown();

        DestroyNameRegistry();

        CoreApi::Shutdown();

        delete g_shaderCompiler;
        g_shaderCompiler = nullptr;

        delete g_materialInstanceCache;
        g_materialInstanceCache = nullptr;

        // Named threads
        delete g_mainThreadInstance;
        g_mainThreadInstance = nullptr;

        delete g_simThreadInstance;
        g_simThreadInstance = nullptr;

        delete g_renderThreadInstance;
        g_renderThreadInstance = nullptr;

        delete g_visThreadInstance;
        g_visThreadInstance = nullptr;

        // Shutdown object container map - destroys all remaining ObjectBase instances
        GetObjectContainerMap().Shutdown();

        // Pools / arenas
        delete g_sceneArena;
        g_sceneArena = nullptr;

        delete g_streamingArena;
        g_streamingArena = nullptr;

        delete g_scenePool;
        g_scenePool = nullptr;

        delete g_streamingPool;
        g_streamingPool = nullptr;

        delete g_assetPool;
        g_assetPool = nullptr;

        delete g_objectPool;
        g_objectPool = nullptr;

#if HYP_WINDOWS
        Win32_CleanupWindowClasses();
#endif // HYP_WINDOWS
    }

    HYP_EXPORT void Hyp_SetGame(Game* pGame)
    {
        AssertOnThread(g_mainThread);

        g_engineDriver->SetGameInstance(pGame);

        Assert(g_simThreadInstance != nullptr);
        g_simThreadInstance->SetGameInstance(pGame);
    }

    HYP_EXPORT int Hyp_LaunchThreads()
    {
        AssertOnThread(g_mainThread);

        Assert(g_engineDriver.IsValid());

        if (!g_mainThreadInstance || !g_mainThreadInstance->IsRunning())
        {
            if (g_engineDriver->StartThreads())
            {
                return 1;
            }
        }

        return 0;
    }

    HYP_EXPORT void Hyp_StopThreads()
    {
        if (g_mainThreadInstance != nullptr)
        {
            g_mainThreadInstance->Stop();
        }

        if (g_engineDriver != nullptr)
        {
            g_engineDriver->RequestStop();
        }
    }

    HYP_EXPORT AppContextBase* Hyp_GetAppContext()
    {
        return g_appContext.Get();
    }

    HYP_EXPORT HWND Hyp_GetHWND(ApplicationWindow* pWindow)
    {
        if (!pWindow)
        {
            return nullptr;
        }

        return pWindow->GetHWND();
    }

#if HYP_MACOS
    HYP_EXPORT void* Hyp_GetNSView(ApplicationWindow* pWindow)
    {
        if (!pWindow)
        {
            return nullptr;
        }

        CocoaApplicationWindow* cocoaWindow = DynamicCast<CocoaApplicationWindow>(pWindow);

        if (!cocoaWindow)
        {
            return nullptr;
        }

        return cocoaWindow->GetNSView();
    }
#endif // HYP_MACOS

    HYP_EXPORT int Hyp_SetMainWindow(AppContextBase* pCtx, ApplicationWindow* pWindow)
    {
        AssertOnThread(g_mainThread);

        Assert(pCtx != nullptr);
        pCtx->SetMainWindow(MakeStrongRef(pWindow));

        return 1;
    }

    HYP_EXPORT ApplicationWindow* Hyp_GetMainWindow(AppContextBase* pCtx)
    {
        AssertOnThread(g_mainThread);

        Assert(pCtx != nullptr);
        return pCtx->GetMainWindow();
    }

    HYP_EXPORT Game* Hyp_CreateGame(const char* gameClassName)
    {
        if (!gameClassName)
        {
            HYP_LOG(Engine, Error, "Failed to create game: gameClassName is NULL!");
            return nullptr;
        }

        const Class* pGameClass = ClassRegistry::GetInstance().GetClass(StringHash(gameClassName));

        if (!pGameClass || !pGameClass->IsDerivedFrom(Game::StaticClass()))
        {
            HYP_LOG(Engine, Error, "Failed to create game: class '{}' not found or is not a subclass of Game", gameClassName);

            return nullptr;
        }

        BoxedValue boxed;
        if (!pGameClass->CreateInstance(boxed) || !boxed.Is<Game>())
        {
            HYP_LOG(Engine, Error, "Failed to create game: could not create instance of class '{}'", gameClassName);
            return nullptr;
        }

        Handle<Game>& gameHandle = boxed.Get<Handle<Game>>();
        Assert(gameHandle.IsValid());

        Game* pGame = static_cast<Game*>(gameHandle.ptr);
        gameHandle.ptr = nullptr; // transfer ownership

        return pGame;
    }

    HYP_EXPORT void Hyp_DestroyGame(Game* pGame)
    {
        if (!pGame)
        {
            return;
        }

        pGame->Release();
    }

    HYP_EXPORT void Hyp_MainThreadUpdate()
    {
        AssertOnThread(g_mainThread);

        g_mainThreadInstance->Update();
    }

#if HYP_DOTNET
    HYP_EXPORT void Hyp_SetInitFromManagedCallback(InitFromManagedCallback callback)
    {
        s_initFromManagedCallback = callback;
    }
#endif // HYP_DOTNET

#if HYP_EDITOR
    using LogCallback = void (*)(
        const char* channel,
        LogLevel level,
        double timestamp,
        const char* fileName,
        int lineNumber,
        const char* text);

    LogCallback g_logCallback = nullptr;
    int g_logRedirectId = -1;

    static bool HandleLogMessage(void* context, const LogChannel& channel, const LogMessage& message)
    {
        if (g_logCallback)
        {
            String text;
            for (const auto& chunk : message.chunks)
            {
                text.Append(chunk);
            }

            g_logCallback(
                channel.name.LookupString(),
                message.level,
                (double)message.timestamp,
                message.fileName,
                message.lineNumber,
                text.Data());
        }

        // allow default logging to continue
        return true;
    }

    HYP_EXPORT void Hyp_RegisterLogCallback(LogCallback callback)
    {
        g_logCallback = callback;

        if (g_logRedirectId == -1)
        {
            g_logRedirectId = Logger::GetInstance().GetOutputStream()->AddRedirect(
                Bitset(~0u), // All channels
                nullptr,
                HandleLogMessage,
                HandleLogMessage // Use same handler for errors for now
            );
        }
    }
#endif // HYP_EDITOR

    HYP_EXPORT int Hyp_ExecuteConsoleCommand(int argc, const char** argv)
    {
        if (argc == 0)
            return 1; // NO COMMAND!

        // parse command string into cli args

        ANSIString commandName = argv[0];
        String commandLine;

        for (int i = 1; i < argc; i++)
        {
            commandLine += argv[i];
            if (i != argc - 1)
            {
                commandLine += ' ';
            }
        }

        // Look for a CVar with the name of commandName
        CVarBase* cvar = CVarManager::GetInstance().FindVar(commandName);
        if (cvar != nullptr)
        {
            if (cvar->SetFromString(commandLine))
            {
                return 0;
            }

            HYP_LOG(Engine, Error, "Failed to set console variable `{}`. Input is not valid.", cvar->name);

            return 1;
        }

        CommandLineArgumentDefinitions argumentDefinitions {};

        const Class* commandletClass = g_appContext->FindCommandletClass(commandName);

        if (commandletClass != nullptr)
        {
            CommandLineArguments args { *commandletClass->GetName() };

            // check for static method GetArgumentDefinitions() on commandlet class to override.
            if (const Method* m = commandletClass->GetMethod("GetArgumentDefinitions"_sh))
            {
                Span<BoxedValue*> args = { nullptr };

                BoxedValue boxed = m->Invoke(args);
                AssertDebug(boxed.Is<CommandLineArgumentDefinitions>());

                if (boxed.Is<CommandLineArgumentDefinitions>())
                {
                    argumentDefinitions = boxed.Get<CommandLineArgumentDefinitions>();
                }
            }

            CommandLineParser parser { &argumentDefinitions };
            TResult<CommandLineArguments> parseResult = parser.Parse(commandLine);

            Result commandletResult = g_appContext->RunCommandlet(
                *commandletClass->GetName(),
                parseResult.GetValue());

            if (commandletResult.HasError())
            {
                HYP_LOG(Engine, Error, "Commandlet execution failed! {}", commandletResult.GetError().GetMessage());

                return 1;
            }

            return 0; // 0 == success to model C main()
        }

#if HYP_EDITOR
        // Try finding an EditorCommand by name.
        const Class* editorCommandClass = ClassRegistry::GetInstance().GetClass(ANSIString("EditorCommand") + commandName, /* ignoreCase */ true);

        if (editorCommandClass != nullptr)
        {
            BoxedValue boxed;

            if (editorCommandClass->CreateInstance(boxed))
            {
                if (boxed.Is<Handle<EditorCommandBase>>())
                {
                    const Handle<EditorCommandBase>& command = boxed.Get<Handle<EditorCommandBase>>();

                    Handle<EditorSubsystem> editorSubsystem = g_editorState->GetEditorSubsystem();

                    if (editorSubsystem.IsValid())
                    {
                        command->SetArguments(Map(commandLine.Split(' '), &String::Trimmed));

                        // Editor commands should be executed on the simulation thread
                        if (IsOnThread(g_simThread))
                        {
                            command->Execute(editorSubsystem);
                        }
                        else
                        {
                            g_simThreadInstance->GetScheduler().Enqueue([editorSubsystem, command]()
                                {
                                    command->Execute(editorSubsystem);
                                }, TaskEnqueueFlags::FIRE_AND_FORGET);
                        }

                        return 0;
                    }

                    HYP_LOG(Engine, Error, "No active editor subsystem; cannot execute editor command.");
                }
                else
                {
                    HYP_LOG(Engine, Error, "Not an instance of EditorCommandBase");
                }
            }
        }
#endif // HYP_EDITOR

        return 1;
    }

#if HYP_ANDROID
    HYP_EXPORT void Hyp_SetAssetManager(void* assetManager)
    {
        g_androidAssetManager = (AAssetManager*)assetManager;
    }

    HYP_EXPORT void Hyp_SetNativeWindow(void* nativeWindow)
    {
        Assert(g_appContext.IsValid());

        if (AndroidAppContext* androidAppContext = DynamicCast<AndroidAppContext>(g_appContext))
        {
            androidAppContext->SetNativeWindow(nativeWindow);
        }
    }

    HYP_EXPORT void Hyp_InputEvent(int type, int action, float x, float y, int iParam)
    {
        if (!g_appContext.IsValid())
            return;

        AndroidAppContext* ctx = DynamicCast<AndroidAppContext>(g_appContext);

        if (ctx == nullptr || ctx->GetMainWindow() == nullptr)
            return;

        AndroidApplicationWindow* window = DynamicCast<AndroidApplicationWindow>(ctx->GetMainWindow());
        if (window == nullptr)
            return;

        Event event;
        if (window->HandleInputEvent(type, action, x, y, iParam, event))
        {
            ctx->EnqueueEvent(std::move(event));
        }
    }
#endif // HYP_ANDROID

    HYP_EXPORT void Hyp_GetAllCVarNames(void* callback, void* userData)
    {
        using CallbackType = void(*)(const char*, void*);
        auto callbackFn = reinterpret_cast<CallbackType>(callback);

        const auto& cvars = CVarManager::GetInstance().cvars;
        for (CVarBase* cvar : cvars)
        {
            if (!cvar)
            {
                continue;
            }

            callbackFn(cvar->name.LookupString(), userData);
        }
    }

    HYP_EXPORT void Hyp_GetAllCommandletNames(void* callback, void* userData)
    {
        using CallbackType = void(*)(const char*, void*);
        auto callbackFn = reinterpret_cast<CallbackType>(callback);

        const Class* commandletBaseClass = ClassRegistry::GetInstance().GetClass("CommandletBase"_sh);
        Assert(commandletBaseClass != nullptr);

        ClassRegistry::GetInstance().ForEachClass([&](const Class* cls) -> IterationResult
        {
            if (cls->IsDerivedFrom(commandletBaseClass))
            {
                String str = cls->GetName().ToString();

                // Conditional: If the name of the class ends with "Commandlet", we strip off that part of the string before handing it over.
                if (str.EndsWith("Commandlet"))
                {
                    str = str.Substr(0, str.Length() - (std::size("Commandlet") - 1));

                    callbackFn(str.Data(), userData);
                }
                else
                {
                    callbackFn(cls->GetName().LookupString(), userData);
                }
            }

            return IterationResult::CONTINUE;
        });
    }

#if HYP_EDITOR
    HYP_EXPORT void Hyp_GetAllEditorCommandNames(void* callback, void* userData)
    {
        using CallbackType = void(*)(const char*, void*);
        auto callbackFn = reinterpret_cast<CallbackType>(callback);

        const Class* editorCommandBaseClass = ClassRegistry::GetInstance().GetClass("EditorCommandBase"_sh);
        Assert(editorCommandBaseClass != nullptr);

        ClassRegistry::GetInstance().ForEachClass([&](const Class* cls) -> IterationResult
        {
            if (cls->IsDerivedFrom(editorCommandBaseClass))
            {
                String str = cls->GetName().ToString();
                if (str.StartsWith("EditorCommand"))
                {
                    str = str.Substr(std::size("EditorCommand") - 1);

                    callbackFn(str.Data(), userData);
                }
            }

            return IterationResult::CONTINUE;
        });
    }
#endif // HYP_EDITOR

}

} // namespace Hyperion
