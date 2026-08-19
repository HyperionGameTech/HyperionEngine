/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <HyperionEngine.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineStats.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/CVarManager.hpp>
#include <Framework/Game.hpp>
#include <Framework/CacheClient.hpp>

#include <Framework/Threads/MainThread.hpp>
#include <Framework/Threads/SimThread.hpp>
#include <Framework/Threads/RenderThread.hpp>
#include <Framework/Threads/RenderWorkerThread.hpp>
#include <Framework/Threads/VisThread.hpp>

#include <Framework/Server/GameServer.hpp>
#include <Framework/Client/GameClient.hpp>

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

#include <Net/NetRequestThread.hpp>

#include <System/MessageBox.hpp>
#include <System/AppContext.hpp>
#include <System/DirectoryInitializer.hpp>

#include <Input/Event.hpp>

#include <Streaming/StreamingManager.hpp>

#include <Rendering/Material.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/DebugDrawer.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderCompiler.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>
#include <Rendering/Shared.hpp>

#include <Scene/ComponentInterface.hpp>

#include <UI/UIDataSource.hpp> // For UIElementFactoryRegistry

#include <Audio/AudioManager.hpp>

#ifdef HYP_VULKAN
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#endif // HYP_VULKAN

#ifdef HYP_EDITOR
#include <Editor/EditorState.hpp>
#include <Editor/EditorCommand.hpp>
#include <Editor/EditorSubsystem.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#endif // HYP_EDITOR

#ifdef HYP_DOTNET
#include <DotNET/DotNETHost.hpp>
#endif // HYP_DOTNET

#ifdef HYP_STEAM_SDK
#include <Steam/Steam.hpp>
#include <Steam/SteamInput.hpp>
#endif // HYP_STEAM_SDK

#ifdef HYP_ANDROID
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
#include <Framework/EngineMemory.inl>
#undef HYP_ENGINE_MEMORY_IMPLEMENTATION

#pragma endregion Memory Pools

ENGINE_API extern void InitializeModule_Engine();
CORE_API extern void InitializeModule_Core();

#ifdef HYP_EDITOR
EDITOR_API extern void InitializeModule_Editor();
#endif // HYP_EDITOR

#ifdef HYP_SCRIPT
SCRIPT_API extern void InitializeModule_Script();
#endif // HYP_SCRIPT

// defined in PlatformUtils.[cpp|mm]
namespace PlatformUtils {
ENGINE_API extern PlatformString GetExecutableAbsolutePath();
ENGINE_API extern bool IsOnBatteryPower();
ENGINE_API extern void InitializeNetwork();
} // namespace PlatformUtils

ENGINE_API Handle<EngineDriver> g_engineDriver;
ENGINE_API Handle<AssetManager> g_assetManager;
ENGINE_API Handle<AudioManager> g_audioManager;
ENGINE_API Handle<AppContextBase> g_appContext;
ENGINE_API Handle<StreamingManager> g_streamingManager;
ENGINE_API Handle<EngineStats> g_engineStats;
ENGINE_API MaterialCache* g_materialCache;
ENGINE_API ShaderCompiler* g_shaderCompiler;
ENGINE_API ShaderManager* g_shaderManager;
ENGINE_API GameServer* g_gameServer;
ENGINE_API GameClient* g_gameClient;

#ifdef HYP_EDITOR
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

namespace {

void HandleFatalError(const char* message)
{
    SystemMessageBox(MessageBoxType::CRITICAL)
        .Title("Fatal error logged!")
        .Text(message)
        .Show();

    debug::TerminateProgram();
}

#if HYP_DOTNET
static InitFromManagedCallback s_initFromManagedCallback = nullptr;
#endif

namespace SignalHandlers
{
static void HandleExit()
{
#ifdef HYP_STEAM_SDK
    Steam::SteamInputManager::GetInstance().Shutdown();
    Steam::Shutdown();
#endif // HYP_STEAM_SDK

#ifdef HYP_WINDOWS
    Win32_CleanupWindowClasses();
#endif // HYP_WINDOWS
}

static void HandleSignal(int signum)
{
    // Handle ctrl-c graceful stop.
    // we want to ensure Hyp_Shutdown() is only ever called from the main thread.
    if (signum == SIGINT)
    {
        auto doGracefulShutdown = []
        {
            Hyp_Shutdown();

            exit(SIGINT);
        };

        ThreadBase* mainThread;
        if (!IsOnThread(g_mainThread)
            && (mainThread = GetThreadById(g_mainThread))
            && mainThread->IsRunning())
        {
            mainThread->GetScheduler().Enqueue(doGracefulShutdown, TaskEnqueueFlags::FIRE_AND_FORGET);

            return;
        }

        doGracefulShutdown();

        return;
    }

    // Call atexit functions
    exit(signum);
}

} // anonymous SignalHandlers 

void InitSignalHandlers()
{
    // Init signal handlers
    signal(SIGINT, SignalHandlers::HandleSignal);
    signal(SIGSEGV, SignalHandlers::HandleSignal);
    atexit(SignalHandlers::HandleExit);
}

void InitThreads()
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

        if (CoreApi::GetCommandLineArguments()["SimulateOnMainThread"].ToBool() || EngineGlobals::IsHeadless())
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

    g_mainThreadInstance = new MainThread;
    SetCurrentThreadObject(g_mainThreadInstance);

    g_renderThreadInstance = new RenderThread;
    g_simThreadInstance = new SimThread;
    g_visThreadInstance = new VisThread;

#if !defined(HYP_IOS) && !defined(HYP_ANDROID)
    if constexpr (NumRendererWorkerThreads != 0)
    {
        g_renderWorkerThreadPool = new RenderWorkerThreadPool(NumRendererWorkerThreads, ThreadPriorityValue::HIGHEST);
    }
#endif // !HYP_IOS && !HYP_ANDROID
}

void InitLogger()
{
    Logger::GetInstance().fatalErrorHook = &HandleFatalError;

    LogChannelRegistrar::GetInstance().RegisterAll();
}

void InitMainWindow()
{
    Assert(g_appContext.IsValid());
    
    const CommandLineArguments& cliArgs = CoreApi::GetCommandLineArguments();

    EnumFlags<WindowFlags> windowFlags = WindowFlags::EVENTS_POLLING;

    if (cliArgs["Headless"].ToBool())
    {
        windowFlags |= WindowFlags::HEADLESS;
    }

    if (cliArgs["HighDPI"].ToBool())
    {
        windowFlags |= WindowFlags::HIGH_DPI;
    }

    if (!(windowFlags & WindowFlags::HEADLESS) && !EngineGlobals::IsEditor())
    {
        Vec2i resolution = { 1920, 1080 };

        if (cliArgs["ResX"].IsNumber())
        {
            resolution.x = cliArgs["ResX"].ToInt32();
        }

        if (cliArgs["ResY"].IsNumber())
        {
            resolution.y = cliArgs["ResY"].ToInt32();
        }

        HYP_LOG(Engine, Info, "Running in windowed mode: {}x{}", resolution.x, resolution.y);

        Handle<ApplicationWindow> window = g_appContext->CreateSystemWindow({ "Hyperion Engine", resolution, windowFlags });

        window->OnClose
            .Bind(window, []()
                    {
                        // shut down application on main window close.
                        g_mainThreadInstance->GetScheduler().Enqueue(
                            []()
                            {
                                Hyp_Shutdown();

                                std::exit(0);
                            },
                            TaskEnqueueFlags::FIRE_AND_FORGET);
                    })
            .Detach();

        Assert(window.IsValid());

        g_appContext->SetMainWindow(window);
    }
}

void LoadShaderPropertyDictionary()
{
    InitShaderPropertyDictionary();

    const FilePath shaderPropsFilePath = EngineGlobals::GetCacheDirectory() / "shaderprops.bin";

    FileByteReader stream { shaderPropsFilePath };

#ifdef HYP_SHIPPING
    Assert(!stream.Eof(), "shaderprops.bin missing - required in shipping builds");
    Assert(ReadShaderPropertyDictionary(stream), "Failed to read shaderprops.bin");
#else
    const bool loaded = !stream.Eof() && ReadShaderPropertyDictionary(stream);

    if (!loaded)
    {
        // If the property cache is missing or stale, we need to also purge the preload cache
        // as they use the same ids
        const FilePath preloadCachePath = EngineGlobals::GetCacheDirectory() / "shaderpreload.bin";

        if (preloadCachePath.Exists() && !preloadCachePath.Remove())
        {
            HYP_LOG(Engine, Warning, "Failed to remove stale shader preload cache at {}", preloadCachePath);
        }
    }
#endif

    stream.Close();

    StaticShaderPropertyId::ResolveAll();
}

} // namespace

extern "C"
{
    static bool s_hypIsInitialized = false;

    HYP_EXPORT int Hyp_IsInitialized()
    {
        return int(s_hypIsInitialized);
    }

    HYP_EXPORT int Hyp_Initialize(int argc, char** argv)
    {
        if (!argv || argc <= 0)
        {
            // we need argc/argv to be passed by the caller
            return false;
        }

        if (s_hypIsInitialized)
        {
            return true; // already initialized
        }

        s_hypIsInitialized = true;

        SetCurrentThreadId(g_mainThread);

        InitializeModule_Core();
        InitializeModule_Engine();

#ifdef HYP_EDITOR
        InitializeModule_Editor();
#endif // HYP_EDITOR

#ifdef HYP_SCRIPT
        InitializeModule_Script();
#endif // HYP_SCRIPT

#if HYP_ANDROID
        const FilePath basePath = FilePath(AndroidAssetPathPrefix);
#else
        const FilePath basePath = FilePath(PlatformUtils::GetExecutableAbsolutePath().ToUtf8()).BasePath();
#endif

        CoreApi::SetExecutablePath(basePath);

        if (!CoreApi::Initialize(argc, argv))
        {
            return false;
        }

        PlatformUtils::InitializeNetwork();

        EngineConfig engineConfig;
        engineConfig.Load();

        CVarManager::GetInstance().InitFromConfig(engineConfig);
        
        InitSignalHandlers();
        InitThreads();
        InitMemoryPools();
        InitNameRegistry();

        ClassRegistry::GetInstance().Initialize();

        InitLogger();

#if HYP_DOTNET && !defined(HYP_COMMANDLET_NAME)
        if (!EngineGlobals::IsCommandlet())
        {
            bool shouldInitializeDotNetHost = true;

#if defined(HYP_DOTNET_ONLY_FOR_EDITOR) && HYP_DOTNET_ONLY_FOR_EDITOR
            shouldInitializeDotNetHost = EngineGlobals::IsEditor();
#endif // HYP_DOTNET_ONLY_FOR_EDITOR

            if (shouldInitializeDotNetHost)
            {
                DotNETHost::GetInstance().Initialize(basePath, /* initFromManaged */ EngineGlobals::IsEditor(), s_initFromManagedCallback);
            }
        }
#endif // HYP_DOTNET

        ComponentInterfaceRegistry::GetInstance().Initialize();

        SharedPtr<NetRequestThread> netRequestThread = MakeShared<NetRequestThread>();
        SetGlobalNetRequestThread(netRequestThread);
        netRequestThread->Start();

        g_engineDriver = MakeHandle<EngineDriver>();

        g_engineStats = MakeHandle<EngineStats>();

        g_streamingManager = MakeHandle<StreamingManager>();
        g_streamingManager->Start();

        g_assetManager = MakeHandle<AssetManager>();
        g_assetManager->Initialize();

#ifdef HYP_EDITOR
        // Create the editor asset registry
        {
            Handle<AssetRegistry> editorRegistry = MakeHandle<AssetRegistry>(
                AssetRegistryId::Editor,
                EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Editor")>());

            editorRegistry->Initialize();

            SetEditorAssetRegistry(editorRegistry);
        }
#endif // HYP_EDITOR

        g_audioManager = MakeHandle<AudioManager>();
        g_audioManager->Initialize();

#ifdef HYP_EDITOR
        g_editorState = MakeHandle<EditorState>();
        g_editorState->Initialize();
#endif // HYP_EDITOR

        g_materialCache = new MaterialCache;

        LoadShaderPropertyDictionary();

        if (EngineGlobals::IsCommandlet())
        {
            const ANSIString commandletName = CoreApi::GetCommandLineArguments()["exec"].ToString().ToAnsi();

            const Class* commandletClass = g_appContext->FindCommandletClass(commandletName);

            if (!commandletClass)
            {
                HYP_LOG(Engine, Error, "Failed to find Commandlet class with name: {}", commandletName);

                Hyp_Shutdown();

                return false;
            }

            // Build cli string from raw args after --exec=<command> OR --exec <command>
            const String commandletNameStr = CoreApi::GetCommandLineArguments()["exec"].ToString();
            Array<String> commandletArgsRaw;

            enum class ExecState
            {
                None,
                FoundFlag,
                Collecting
            };

            ExecState execState = ExecState::None;

            for (int i = 1; i < argc; i++)
            {
                String arg = String(argv[i]);

                switch (execState)
                {
                case ExecState::None:
                    if (arg.StartsWith("--exec="))
                    {
                        if (arg.Substr(7) == commandletNameStr)
                        {
                            execState = ExecState::Collecting;
                        }
                    }
                    else if (arg == "--exec")
                    {
                        execState = ExecState::FoundFlag;
                    }
                    break;
                case ExecState::FoundFlag:
                    execState = ExecState::Collecting; // skip the commandlet name
                    break;
                case ExecState::Collecting:
                    commandletArgsRaw.PushBack(std::move(arg));
                    break;
                }
            }

            String cliString = commandletNameStr + ' ' + String::Join(commandletArgsRaw, " ");

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

                Hyp_Shutdown();

                return false;
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

            return !commandletResult.HasError();
        }
        
#if HYP_WINDOWS
        g_appContext = MakeHandle<Win32AppContext>("Hyperion", CoreApi::GetCommandLineArguments());
#elif HYP_MACOS
        g_appContext = MakeHandle<CocoaAppContext>("Hyperion", CoreApi::GetCommandLineArguments());
#elif HYP_ANDROID
        g_appContext = MakeHandle<AndroidAppContext>("Hyperion", CoreAPi::GetCommandLineArguments());
#elif HYP_IOS
        g_appContext = MakeHandle<IOSAppContext>("Hyperion", CoreApi::GetCommandLineArguments());
#else  // !HYP_WINDOWS && !HYP_MACOS && !HYP_ANDROID && !HYP_IOS
        HYP_FAIL("AppContext not implemented for this platform");
#endif // HYP_WINDOWS || HYP_MACOS || HYP_ANDROID || HYP_IOS
        
        if (EngineGlobals::IsServer())
        {
            g_gameServer = new GameServer;

            if (Result listenResult = g_gameServer->Start(EngineGlobals::GetGameServerPort()); listenResult.HasError())
            {
                HYP_LOG(Engine, Error, "Failed to start game server: {}", listenResult.GetError().GetMessage());

                Hyp_Shutdown();

                return false;
            }
        }
        else
        {
            g_gameClient = new GameClient;
        }

        if (!EngineGlobals::IsHeadless())
        {
            InitMainWindow();

#ifdef HYP_STEAM_SDK
            Steam::Initialize();
            Steam::SteamInputManager::GetInstance().Initialize();
#endif // HYP_STEAM_SDK
        }

        // must start after net request thread
        if (CoreApi::IsProfilingEnabled())
        {
            StartProfilerConnectionThread(ProfilerConnectionParams {
                /* endpointUrl */ CoreApi::GetCommandLineArguments()["TraceURL"].ToString(),
                /* enabled */ true
            });
        }

        g_engineDriver->Initialize();

        return true;
    }

    HYP_EXPORT void Hyp_Shutdown()
    {
        AssertOnThread(g_mainThread);

        if (!s_hypIsInitialized)
        {
            return;
        }

        s_hypIsInitialized = false;

        if (g_gameServer != nullptr)
        {
            g_gameServer->Stop();

            delete g_gameServer;
            g_gameServer = nullptr;
        }

        if (g_gameClient != nullptr)
        {
            g_gameClient->Disconnect();

            delete g_gameClient;
            g_gameClient = nullptr;
        }

        g_engineDriver->Shutdown();
    
#ifdef HYP_STEAM_SDK
        Steam::SteamInputManager::GetInstance().Shutdown();
        Steam::Shutdown();
#endif // HYP_STEAM_SDK

#if HYP_DOTNET
        DotNETHost::GetInstance().Shutdown();
#endif // HYP_DOTNET
    
        if (g_renderThreadInstance != nullptr && g_renderThreadInstance->IsRunning())
        {
            g_renderThreadInstance->Stop();
        }

        if (g_simThreadInstance != nullptr && g_simThreadInstance->IsRunning())
        {
            g_simThreadInstance->Stop();
        }
        
        g_mainThreadInstance->Stop();
        
        g_renderThreadInstance->Join();
        g_renderThread = g_mainThread;

        g_simThreadInstance->Join();
        g_simThread = g_mainThread;
        
        if (g_renderWorkerThreadPool != nullptr && g_renderWorkerThreadPool->IsRunning())
        {
            g_renderWorkerThreadPool->Stop();
        }

        if (TaskSystem::GetInstance().IsRunning())
        {
            TaskSystem::GetInstance().Stop();
        }

        // must stop before net request thread
        StopProfilerConnectionThread();

        if (SharedPtr<NetRequestThread> netRequestThread = GetGlobalNetRequestThread())
        {
            if (netRequestThread->IsRunning())
            {
                netRequestThread->Stop();
            }

            if (netRequestThread->CanJoin())
            {
                netRequestThread->Join();
            }

            SetGlobalNetRequestThread(nullptr);
        }

        { // shut down AssetRegistry instances
            ClearAssetRegistryStack();

            if (Handle<AssetRegistry> engineRegistry = GetEngineAssetRegistry(); engineRegistry.IsValid())
            {
                engineRegistry->Shutdown();
                SetEngineAssetRegistry(Handle<AssetRegistry>::Null());
            }

#ifdef HYP_EDITOR
            if (Handle<AssetRegistry> editorRegistry = GetEditorAssetRegistry(); editorRegistry.IsValid())
            {
                editorRegistry->Shutdown();
                SetEditorAssetRegistry(Handle<AssetRegistry>::Null());
            }
#endif // HYP_EDITOR
        }

        g_streamingManager->Stop();
        g_streamingManager.Reset();

        g_audioManager->Shutdown();
        g_audioManager.Reset();

        g_assetManager.Reset();
        g_engineDriver.Reset();
        g_engineStats.Reset();

#ifdef HYP_EDITOR
        g_editorState.Reset();
#endif // HYP_EDITOR

        g_appContext.Reset();

        ComponentInterfaceRegistry::GetInstance().Shutdown();

        UIElementFactoryRegistry::GetInstance().Shutdown();

        DestroyNameRegistry();

        CoreApi::Shutdown();

        delete g_materialCache;
        g_materialCache = nullptr;

        // Named threads
        delete g_mainThreadInstance;
        g_mainThreadInstance = nullptr;

        delete g_simThreadInstance;
        g_simThreadInstance = nullptr;

        delete g_renderThreadInstance;
        g_renderThreadInstance = nullptr;

        delete g_visThreadInstance;
        g_visThreadInstance = nullptr;

        delete g_renderWorkerThreadPool;
        g_renderWorkerThreadPool = nullptr;

        // Shutdown object container map - destroys all remaining ObjectBase instances
        // @TODO Move init/shutdown into CoreApi Initialize and Dhutdown
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

        Assert(s_hypIsInitialized);

        g_engineDriver->SetGameInstance(pGame);

        Assert(g_simThreadInstance != nullptr);
        g_simThreadInstance->SetGameInstance(pGame);
    }

    HYP_EXPORT int Hyp_LaunchThreads()
    {
        AssertOnThread(g_mainThread);

        Assert(s_hypIsInitialized);

        if (!g_mainThreadInstance || !g_mainThreadInstance->IsRunning())
        {
            const bool success = g_engineDriver->StartThreads();

            if (!success)
            {
                return false;
            }
        }

        return true;
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

        return true;
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

#ifdef HYP_EDITOR
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
            g_logRedirectId = Logger::GetInstance().AddRedirect(
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
        {
            return 1; // NO COMMAND!
        }

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
            TResult<CommandLineArguments> parseResult = parser.Parse(commandName + " " + commandLine);

            if (parseResult.HasError())
            {
                HYP_LOG(Engine, Error, "Failed to parse commandlet arguments: {}", parseResult.GetError().GetMessage());

                return 1;
            }

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

#ifdef HYP_EDITOR
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
                        command->SetArguments(MapToArray(commandLine.Split(' '), &String::Trimmed));

                        // Editor commands should be executed on the simulation thread
                        if (IsOnThread(g_simThread))
                        {
                            command->Execute(editorSubsystem);
                        }
                        else
                        {
                            g_simThreadInstance->GetScheduler().Enqueue(
                                [editorSubsystem, command]()
                                {
                                    command->Execute(editorSubsystem);
                                },
                                TaskEnqueueFlags::FIRE_AND_FORGET);
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

#ifdef HYP_ANDROID
    HYP_EXPORT void Hyp_SetAssetManager(void* assetManager)
    {
        g_androidAssetManager = (AAssetManager*)assetManager;
    }

    HYP_EXPORT void Hyp_SetNativeWindow(void* nativeWindow, int width, int height)
    {
        Assert(g_appContext.IsValid());

        if (AndroidAppContext* androidAppContext = DynamicCast<AndroidAppContext>(g_appContext))
        {
            androidAppContext->SetNativeWindow(nativeWindow, Vec2i { width, height });
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

    HYP_EXPORT void Hyp_TextInputEvent(const char* text)
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
        if (window->HandleTextInputEvent(String(text), event))
        {
            ctx->EnqueueEvent(std::move(event));
        }
    }
#endif // HYP_ANDROID

    HYP_EXPORT void Hyp_GetAllCVarNames(void* callback, void* userData)
    {
        using CallbackType = void (*)(const char*, void*);
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
        using CallbackType = void (*)(const char*, void*);
        auto callbackFn = reinterpret_cast<CallbackType>(callback);

        const Class* commandletBaseClass = ClassRegistry::GetInstance().GetClass("CommandletBase"_sh);
        Assert(commandletBaseClass != nullptr);

        auto Predicate = [&](const Class* cls) -> IterationResult
        {
            if (cls->IsDerivedFrom(commandletBaseClass))
            {
                String str = cls->GetName().ToString();

                // Conditional: If the name of the class ends with "Commandlet", we strip off that part of the string before handing it over.
                if (str.EndsWith("Commandlet"))
                {
                    str = str.Substr(0, str.Length() - (GetArrayCount("Commandlet") - 1));

                    callbackFn(str.Data(), userData);
                }
                else
                {
                    callbackFn(cls->GetName().LookupString(), userData);
                }
            }

            return IterationResult::CONTINUE;
        };

        ClassRegistry::GetInstance().ForEachClass(Predicate);
    }

#ifdef HYP_EDITOR
    HYP_EXPORT void Hyp_GetAllEditorCommandNames(void* callback, void* userData)
    {
        using CallbackType = void (*)(const char*, void*);
        auto callbackFn = reinterpret_cast<CallbackType>(callback);

        const Class* editorCommandBaseClass = ClassRegistry::GetInstance().GetClass("EditorCommandBase"_sh);
        Assert(editorCommandBaseClass != nullptr);

        auto Predicate = [&](const Class* cls) -> IterationResult
        {
            if (cls->IsDerivedFrom(editorCommandBaseClass))
            {
                String str = cls->GetName().ToString();
                if (str.StartsWith("EditorCommand"))
                {
                    str = str.Substr(GetArrayCount("EditorCommand") - 1);

                    callbackFn(str.Data(), userData);
                }
            }

            return IterationResult::CONTINUE;
        };

        ClassRegistry::GetInstance().ForEachClass(Predicate);
    }

    HYP_EXPORT void Hyp_GetAllDerivedClassNames(const char* baseClassName, void* callback, void* userData)
    {
        using CallbackType = void (*)(const char*, void*);
        auto callbackFn = reinterpret_cast<CallbackType>(callback);

        const Class* baseClass = ClassRegistry::GetInstance().GetClass(ANSIStringView(baseClassName));

        if (!baseClass)
            return;

        auto Predicate = [&](const Class* cls) -> IterationResult
        {
            if (cls->IsDerivedFrom(baseClass) && !cls->IsAbstract())
            {
                callbackFn(cls->GetName().LookupString(), userData);
            }

            return IterationResult::CONTINUE;
        };

        ClassRegistry::GetInstance().ForEachClass(Predicate);
    }

    HYP_EXPORT bool Hyp_CreateInstanceOfClass(const char* className, BoxedValue* outBoxed)
    {
        Assert(outBoxed != nullptr);

        const Class* cls = ClassRegistry::GetInstance().GetClass(ANSIStringView(className));

        if (!cls)
            return false;

        return cls->CreateInstance(*outBoxed);
    }

#endif // HYP_EDITOR
}

} // namespace Hyperion
