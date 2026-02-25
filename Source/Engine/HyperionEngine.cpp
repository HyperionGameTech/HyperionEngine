/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <HyperionEngine.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>
#include <engine/Game.hpp>

#include <engine/threads/MainThread.hpp>
#include <engine/threads/SimThread.hpp>
#include <engine/threads/RenderThread.hpp>
#include <engine/threads/VisThread.hpp>

#include <asset/Assets.hpp>

#include <Core/Core.hpp>

#include <Core/reflection/ClassRegistry.hpp>
#include <Core/reflection/Class.hpp>
#include <Core/reflection/TypeInfo.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/threading/Threads.hpp>
#include <Core/threading/TaskSystem.hpp>

#include <Core/memory/allocator/ArenaAllocator.hpp>
#include <Core/memory/pool/Pool.hpp>

#include <Core/cli/CommandLine.hpp>

#include <engine/console/ConsoleCommandManager.hpp>

#include <system/MessageBox.hpp>
#include <system/AppContext.hpp>
#include <system/DirectoryInitializer.hpp>

#include <streaming/StreamingManager.hpp>

#include <rendering/Material.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/DebugDrawer.hpp>

#include <rendering/util/DeletionQueue.hpp>
#include <rendering/util/ShaderCompiler.hpp>
#include <rendering/util/ShaderPropertyDictionary.hpp>

#include <scene/ComponentInterface.hpp>

#include <audio/AudioManager.hpp>

#if HYP_VULKAN
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#endif

#if HYP_EDITOR
#include <editor/EditorState.hpp>
#endif

#if HYP_DOTNET
#include <dotnet/DotNETHost.hpp>
#endif

/// ========== If this include is missing, you need to run the CodeGen tool (instructions in doc/CompilingTheEngine.md) ==========
#include <CodeGenOutput.inc>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Engine);

#pragma region Memory Pools

#define HYP_ENGINE_MEMORY_IMPLEMENTATION 1
#include <engine/EngineMemory.inc>
#undef HYP_ENGINE_MEMORY_IMPLEMENTATION

#pragma endregion Memory Pools

// defined in ClassDecls.cpp
extern void InitClassDecls();

Handle<EngineDriver> g_engineDriver;
Handle<AssetManager> g_assetManager;
Handle<AudioManager> g_audioManager;
Handle<AppContextBase> g_appContext;
Handle<StreamingManager> g_streamingManager;
Handle<EngineStats> g_engineStats;
MaterialCache* g_materialCache;
ShaderCompiler* g_shaderCompiler;

#ifdef HYP_EDITOR
Handle<EditorState> g_editorState;
#endif

MainThread* g_mainThreadInstance;
SimThread* g_simThreadInstance;
RenderThread* g_renderThreadInstance;
VisThread* g_visThreadInstance;

Game* g_gameInstance; // active game instance, read/write only from the main thread

#if HYP_VULKAN
VulkanRenderInterface* g_renderInterface;
#elif HYP_DX12
DX12RenderInterface* g_renderInterface;
#endif

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
#else
    // shouldn't be used in non-editor builds, so just return executable path to avoid issues with appending paths
    HYP_LOG(Engine, Warning, "GetLibraryDirectory() called in non-editor build; returning executable path instead");

    static const FilePath s_emptyPath = CoreApi::GetExecutablePath();
    Assert(s_emptyPath.Length() != 0); // don't want to return empty path which will cause appending to give root-level paths.
    return s_emptyPath;
#endif
}

HYP_EXPORT const FilePath& GetProjectsDirectory()
{
    // @TODO Use configuration value for this path. can be in Documents folder eg

    static DirectoryInitializer<HYP_STATIC_STRING("Projects"), /* RelativeToExecutablePath */ false> s_projectsDirectory;
    return s_projectsDirectory.path;
}

// Directory for cached data (shader bundles, compiled scripts, etc.) Expected to be compiled into the asset registry in production builds
static bool s_cacheDirectoryInit = false;
static SharedMutex s_cacheDirectoryMutex;

HYP_EXPORT const FilePath& GetCacheDirectory()
{
    static const ConfigurationValue& s_cfgCacheDirectory = CoreApi::GetGlobalConfig().Get("App.Cache.BaseDirectory");
    static const ConfigurationValue& s_cfgCachePageSize = CoreApi::GetGlobalConfig().Get("App.Cache.PageSize");

    static const FilePath s_cacheDirectory = CoreApi::GetExecutablePath() / s_cfgCacheDirectory.AsString().ToUtf8();

    TSharedLock sharedLock(s_cacheDirectoryMutex);

    if (s_cacheDirectoryInit)
        return s_cacheDirectory;

    sharedLock.Reset();

    TUniqueLock uniqueLock(s_cacheDirectoryMutex);

    if (s_cacheDirectoryInit)
        return s_cacheDirectory;

    if (!s_cfgCachePageSize.IsNumber() || s_cfgCachePageSize.AsNumber() < 1024 * 1024)
    {
        ConfigurationTable newConfigurationTable;
        newConfigurationTable.Set("App.Cache.PageSize", ConfigurationValue(BlobStorage::DefaultPageSize));

        CoreApi::UpdateGlobalConfig(newConfigurationTable);
    }

    if (!s_cacheDirectory.Exists() && !s_cacheDirectory.MkDir())
    {
        HYP_FAIL("Failed to initialize cache storage directory {}!", s_cacheDirectory);
    }

    s_cacheDirectoryInit = true;

    return s_cacheDirectory;
}

// Editor build only
HYP_EXPORT const FilePath& GetTempDirectory()
{
    static DirectoryInitializer<HYP_STATIC_STRING("Temp"), /* RelativeToExecutablePath */ true> s_tempDirectory;
    return s_tempDirectory.path;
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
        g_renderThread = StaticThreadId(mainThreadIndex, NAME("RenderThread"));
        g_simThread = StaticThreadId(NAME("SimThread"));
    }
    else
    {
        g_renderThread = StaticThreadId(NAME("RenderThread"));

        if (CoreApi::GetCommandLineArguments()["SimulateOnMainThread"].ToBool())
        {
            g_simThread = StaticThreadId(mainThreadIndex, NAME("SimThread"));
        }
        else
        {
            g_simThread = StaticThreadId(NAME("SimThread"));
        }
    }

    if (CoreApi::GetCommandLineArguments()["DedicatedVisThread"].ToBool())
    {
        g_visThread = StaticThreadId(NAME("VisThread"));
    }
    else
    {
        // use sim thread for visibility state updates
        g_visThread = g_simThread;
    }

    g_mainThreadInstance = new MainThread();
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

    FileBufferedReaderSource source { GetCacheDirectory() / "ShaderProperties.bin" };
    BufferedByteReader br { &source };

    if (br.IsOpen())
    {
        ReadShaderPropertyDictionary(br);
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
        
        InitClassDecls();

        CoreApi::SetConfigDirectory(GetConfigDirectory());

        if (!CoreApi::Initialize(argc, argv))
        {
            return 0;
        }

        InitThreads();
        InitMemoryPools();
        InitNameRegistry();

        ClassRegistry::GetInstance().Initialize();

        InitLogger();

        const CommandLineArguments& cliArgs = CoreApi::GetCommandLineArguments();

        const FilePath basePath = FilePath(cliArgs.GetCommand()).BasePath();
        CoreApi::SetExecutablePath(basePath);
        
        const bool isEditor = cliArgs["Editor"].ToBool();

#if HYP_DOTNET
        // dont initialize hostfxr if running from editor,
        // leads to type identity issues with managed types
        // due to multiple runtimes being loaded.
        DotNETHost::GetInstance().Initialize(basePath, /* initFromManaged */ isEditor, s_initFromManagedCallback);
#endif

        ConsoleCommandManager::GetInstance().Initialize();
        TaskSystem::GetInstance().Start();

        g_engineDriver = MakeHandle<EngineDriver>();

        g_engineStats = MakeHandle<EngineStats>();
        InitObject(g_engineStats);

        g_streamingManager = MakeHandle<StreamingManager>();
        InitObject(g_streamingManager);
        g_streamingManager->Start();

        g_assetManager = MakeHandle<AssetManager>();
        InitObject(g_assetManager);

        g_audioManager = MakeHandle<AudioManager>();
        InitObject(g_audioManager);

#if HYP_EDITOR
        g_editorState = MakeHandle<EditorState>();
        InitObject(g_editorState);
#endif

        g_materialCache = new MaterialCache;

        LoadShaderPropertyDictionary();

        g_shaderCompiler = new ShaderCompiler;
        if (!g_shaderCompiler->LoadShaderDefinitions())
        {
            HYP_LOG(Engine, Error, "Failed to load shader definitions!");
        }

        ComponentInterfaceRegistry::GetInstance().Initialize();

#if HYP_WINDOWS
        g_appContext = MakeHandle<Win32AppContext>("Hyperion", cliArgs);
#elif HYP_MACOS
        g_appContext = MakeHandle<CocoaAppContext>("Hyperion", cliArgs);
#elif HYP_SDL
        g_appContext = MakeHandle<SDLAppContext>("Hyperion", cliArgs);
#else
        HYP_FAIL("AppContext not implemented for this platform");
#endif

        const bool isCommandlet = cliArgs["Commandlet"].ToBool();

        Vec2i resolution = { 1280, 720 };

        EnumFlags<WindowFlags> windowFlags = WindowFlags::HIGH_DPI | WindowFlags::EVENTS_POLLING;

        if (cliArgs["Headless"].ToBool() || isCommandlet)
        {
            windowFlags |= WindowFlags::HEADLESS;
        }

        if (cliArgs["ResX"].IsNumber())
        {
            resolution.x = cliArgs["ResX"].ToInt32();
        }

        if (cliArgs["ResY"].IsNumber())
        {
            resolution.y = cliArgs["ResY"].ToInt32();
        }

        if (!(windowFlags & WindowFlags::HEADLESS))
        {
            HYP_LOG(Engine, Info, "Running in windowed mode: {}x{}", resolution.x, resolution.y);

            Handle<ApplicationWindow> window = g_appContext->CreateSystemWindow({ "Hyperion Engine", resolution, windowFlags });

            DelegateHandler* onCloseHandle = new DelegateHandler();
            
            *onCloseHandle = window->OnClose.Bind([onCloseHandle]()
                {
                    // shut down application on main window close.
                    Hyp_Shutdown();

                    delete onCloseHandle;

                    std::exit(0);
                });

            Assert(window.IsValid());

            g_appContext->SetMainWindow(window);
        }
        else
        {
            HYP_LOG(Engine, Info, "Running in headless mode");
        }

        InitObject(g_engineDriver);
        
        if (isCommandlet)
        {
            const String commandletName = cliArgs["Commandlet"].ToString();

            CommandLineArguments commandletArgs = CommandLineArguments::Merge(
                CoreApi::DefaultCommandLineArgumentDefinitions(),
                CommandLineArguments { commandletName },
                cliArgs);

            commandletArgs.Delete("Commandlet");

            Result commandletResult = g_appContext->RunCommandlet(
                CreateNameFromDynamicString(commandletName),
                commandletArgs);

            if (commandletResult.HasError())
            {
                HYP_LOG(Engine, Error, "Commandlet execution failed! {}", commandletResult.GetError().GetMessage());
            }

            ThreadSleep(1000);

            Hyp_Shutdown();

            std::exit(commandletResult.HasError() ? 1 : 0);

            return 0;
        }

        return 1;
    }

    HYP_EXPORT void Hyp_Shutdown()
    {
        AssertOnThread(g_mainThread);

        Assert(
            g_engineDriver != nullptr,
            "Hyperion not initialized!");

        g_engineDriver->RequestStop();

        g_mainThreadInstance->Stop();

        g_renderThreadInstance->Join();
        g_renderThread = g_mainThread;

        g_simThreadInstance->Join();
        g_simThread = g_mainThread;

        g_engineDriver->FinalizeStop();

        g_streamingManager->Stop();
        g_streamingManager.Reset();

        g_assetManager.Reset();
        g_audioManager.Reset();
        g_engineStats.Reset();

#if HYP_EDITOR
        g_editorState.Reset();
#endif

        g_engineDriver.Reset();
        g_appContext.Reset();

        ComponentInterfaceRegistry::GetInstance().Shutdown();
        ConsoleCommandManager::GetInstance().Shutdown();

#if HYP_DOTNET
        DotNETHost::GetInstance().Shutdown();
#endif

        if (TaskSystem::GetInstance().IsRunning())
        {
            TaskSystem::GetInstance().Stop();
        }

        DeletionQueue::GetInstance().Shutdown();

        DestroyNameRegistry();

        CoreApi::Shutdown();

        delete g_shaderCompiler;
        g_shaderCompiler = nullptr;

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

        delete g_taskPool;
        g_taskPool = nullptr;

        delete g_resourcePool;
        g_resourcePool = nullptr;

        delete g_assetPool;
        g_assetPool = nullptr;

        delete g_renderPool;
        g_renderPool = nullptr;

        delete g_objectPool;
        g_objectPool = nullptr;

#if HYP_WINDOWS
        Win32_CleanupWindowClasses();
#endif
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

        Assert(g_engineDriver != nullptr && g_engineDriver->IsReady());

        if (!g_mainThreadInstance || !g_mainThreadInstance->IsRunning())
        {
            return int(g_engineDriver->StartThreads());
        }

        return 0;
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

        CocoaApplicationWindow* cocoaWindow = ObjCast<CocoaApplicationWindow>(pWindow);

        if (!cocoaWindow)
        {
            return nullptr;
        }

        return cocoaWindow->GetNSView();
    }
#endif

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
        AssertDebug(gameHandle.IsValid());

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

        pGame->GetObjectHeader_Internal()->DecRefStrong();
    }

    HYP_EXPORT void Hyp_MainThreadUpdate()
    {
        AssertOnThread(g_mainThread);

        g_mainThreadInstance->Update();
    }

    HYP_EXPORT void Hyp_SetInitFromManagedCallback(InitFromManagedCallback callback)
    {
        s_initFromManagedCallback = callback;
    }

#if HYP_EDITOR
    using LogCallback = void (*)(
        const char* channel,
        int level,
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
                (int)message.level,
                (double)message.timestamp,
                message.fileName,
                message.lineNumber,
                text.Data());
        }

        // allow default logging to continue
        return true;
    }

    HYP_EXPORT void Editor_RegisterLogCallback(LogCallback callback)
    {
        g_logCallback = callback;

        //if (g_logRedirectId == -1)
        //{
        //    g_logRedirectId = Logger::GetInstance().GetOutputStream()->AddRedirect(
        //        Bitset(~0u), // All channels
        //        nullptr,
        //        HandleLogMessage,
        //        HandleLogMessage // Use same handler for errors for now
        //    );
        //}
    }

    HYP_EXPORT void Editor_ExecuteConsoleCommand(const char* command)
    {
        if (!command)
            return;

        ConsoleCommandManager::GetInstance().ExecuteCommand(command);
    }
#endif
}

} // namespace Hyperion
