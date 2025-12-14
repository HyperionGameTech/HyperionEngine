/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <HyperionEngine.hpp>

#include <asset/Assets.hpp>

#include <dotnet/DotNETHost.hpp>

#include <core/Core.hpp>

#include <core/reflection/ClassRegistry.hpp>
#include <core/reflection/Class.hpp>
#include <core/reflection/TypeInfo.hpp>
#include <core/reflection/Handle.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/memory/allocator/ArenaAllocator.hpp>
#include <core/memory/pool/Pool.hpp>

#include <core/logging/Logger.hpp>

#include <core/cli/CommandLine.hpp>

#include <console/ConsoleCommandManager.hpp>

#include <system/MessageBox.hpp>
#include <system/App.hpp>
#include <system/AppContext.hpp>

#include <streaming/StreamingManager.hpp>

#include <rendering/Material.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

#ifdef HYP_VULKAN
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#endif

#ifdef HYP_EDITOR
#include <editor/EditorState.hpp>
#endif

#include <scene/ComponentInterface.hpp>

#include <audio/AudioManager.hpp>

#include <script/HypScript.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineMemory.hpp>
#include <engine/EngineStats.hpp>

#include <engine/threads/MainThread.hpp>
#include <engine/threads/GameThread.hpp>
#include <engine/threads/RenderThread.hpp>

#include <game/Game.hpp>

#ifdef HYP_LIBUI
#include <ui.h>
#endif

/// ========== If this include is missing, you need to run HypBuildTool (instructions in doc/CompilingTheEngine.md) ==========
#include <BuildToolOutput.inc>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Engine);

#pragma region Memory Pools

static constexpr SizeType ObjectPoolBlockSize = 64 * 1024 * 1024;
static constexpr SizeType RenderPoolBlockSize = 64 * 1024 * 1024;
static constexpr SizeType FramePoolBlockSize = 1 * 1024 * 1024;
static constexpr SizeType ScenePoolBlockSize = 8 * 1024 * 1024;
static constexpr SizeType TaskPoolBlockSize = 4 * 1024 * 1024;
static constexpr SizeType ResourcePoolBlockSize = 8 * 1024 * 1024;
static constexpr SizeType AssetPoolBlockSize = 16 * 1024 * 1024;
static constexpr SizeType StreamingPoolBlockSize = 16 * 1024 * 1024;
static constexpr SizeType ScriptPoolBlockSize = 16 * 1024 * 1024;

static constexpr SizeType SceneArenaSize = 1 * 1024 * 1024;
static constexpr SizeType StreamingArenaSize = 1 * 1024 * 1024;

HYP_EXPORT Pool* g_objectPool;
HYP_EXPORT Pool* g_renderPool;
HYP_EXPORT Pool* g_framePools[RingBufferDepth];
HYP_EXPORT Pool* g_scenePool;
HYP_EXPORT Pool* g_taskPool;
HYP_EXPORT Pool* g_resourcePool;
HYP_EXPORT Pool* g_assetPool;
HYP_EXPORT Pool* g_streamingPool;
HYP_EXPORT Pool* g_scriptPool;

HYP_EXPORT TArena<RenderAllocator>* g_renderArena;
HYP_EXPORT TArena<SceneAllocator>* g_sceneArena;
HYP_EXPORT TArena<StreamingAllocator>* g_streamingArena;

Pool* const* g_enginePools[EPN_MAX] = {
    &g_objectPool, // EPN_CORE
    &g_renderPool, // EPN_RENDER
    &g_scenePool   // EPN_SCENE
};

HYP_EXPORT Pool* GetCurrentFramePool()
{
    return g_framePools[RenderApi::GetRingIndex()];
}

// defined in ClassDecls.cpp
HYP_EXPORT extern void InitializeClassDeclarations();

#pragma endregion Memory Pools

Handle<EngineDriver> g_engineDriver;
Handle<AssetManager> g_assetManager;
Handle<EditorState> g_editorState;
Handle<AppContextBase> g_appContext;
Handle<StreamingManager> g_streamingManager;
Handle<EngineStats> g_engineStats;
Handle<Logger> g_logger;
ShaderManager* g_shaderManager;
MaterialCache* g_materialSystem;
SafeDeleter* g_safeDeleter;
RenderGlobalState* g_renderGlobalState;
ShaderCompiler* g_shaderCompiler;
Handle<InputManager> g_inputManager;

MainThread* g_mainThreadInstance;
GameThread* g_gameThreadInstance;
RenderThread* g_renderThreadInstance;

#ifdef HYP_VULKAN
VulkanRenderBackend* g_renderBackend;
#endif

static void HandleFatalError(const char* message)
{
    SystemMessageBox(MessageBoxType::CRITICAL)
        .Title("Fatal error logged!")
        .Text(message)
        .Show();

    debug::TerminateProgram();
}

template <auto DirectoryStaticString, bool RelativeToExecutablePath = true>
struct DirectoryInitializer
{
    FilePath path;

    DirectoryInitializer()
    {
#ifdef HYP_ROOT_DIR
        // In non-debug modes, we always want resource directories to be relative to the executable path
        if (!RelativeToExecutablePath)
        {
            path = FilePath(HYP_ROOT_DIR) / DirectoryStaticString.Data();
        }
        else
#endif
        {
            path = CoreApi_GetExecutablePath() / DirectoryStaticString.Data();
        }

        if (!path.Exists())
        {
            if (!path.MkDir())
            {
                HYP_FAIL("Failed to create resource directory: {}", path.Data());
            }
        }

        Assert(path.Exists() && path.IsDirectory(), "Resource directory does not exist or is not a directory: {}", path.Data());
        Assert(path.CanRead(), "Resource directory is not readable: {}", path.Data());
        Assert(path.CanWrite(), "Resource directory is not writable: {}", path.Data());
    }
};

// Directory for data to be imported into editor builds
HYP_EXPORT const FilePath& GetResourceDirectory()
{
#ifdef HYP_EDITOR
    static DirectoryInitializer<HYP_STATIC_STRING("res"), /* RelativeToExecutablePath */ false> s_resourceDirectory;
    return s_resourceDirectory.path;
#else
    // shouldn't be used in non-editor builds, so just return executable path to avoid issues with appending paths
    HYP_LOG(Engine, Warning, "GetResourceDirectory() called in non-editor build; returning executable path instead");

    static const FilePath s_emptyPath = CoreApi_GetExecutablePath();
    Assert(s_emptyPath.Length() != 0); // don't want to return empty path which will cause appending to give root-level paths.
    return s_emptyPath;
#endif
}

// Directory for cached data (shader bundles, compiled scripts, etc.) Expected to be compiled into the asset registry in production builds
HYP_EXPORT const FilePath& GetCacheDirectory()
{
    static DirectoryInitializer<HYP_STATIC_STRING("Cache")> s_resourceDirectory;
    return s_resourceDirectory.path;
}

// Directory for temporary data (intermediate compilation outputs, etc.) Will be not be used in production builds
HYP_EXPORT const FilePath& GetTempDirectory()
{
    static DirectoryInitializer<HYP_STATIC_STRING("Temp")> s_resourceDirectory;
    return s_resourceDirectory.path;
}

static void (*s_initFromManagedCallback)() = nullptr;

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

        // load generated class declarations
        InitializeClassDeclarations();

        if (!CoreApi_Initialize(argc, argv))
        {
            return 0;
        }

        if (CoreApi_GetCommandLineArguments()["RenderOnMainThread"].ToBool(true))
        {
            g_renderThread = g_mainThread;
        }
        else
        {
            // create a separate thread for rendering to
            g_renderThread = StaticThreadId(NAME("Render"));
        }

        g_objectPool = new Pool(ObjectPoolBlockSize, PF_NONE);
        g_renderPool = new Pool(RenderPoolBlockSize, PF_NONE, g_renderThread);

        for (uint32 i = 0; i < RingBufferDepth; i++)
        {
            g_framePools[i] = new Pool(FramePoolBlockSize, PF_NONE);
        }

        g_scenePool = new Pool(ScenePoolBlockSize, PF_THREAD_SAFE);
        g_taskPool = new Pool(TaskPoolBlockSize, PF_THREAD_SAFE);
        g_resourcePool = new Pool(ResourcePoolBlockSize, PF_THREAD_SAFE);
        g_assetPool = new Pool(AssetPoolBlockSize, PF_THREAD_SAFE);
        g_streamingPool = new Pool(StreamingPoolBlockSize, PF_THREAD_SAFE);
        g_scriptPool = new Pool(ScriptPoolBlockSize, PF_NONE, g_gameThread);

        g_sceneArena = new TArena<SceneAllocator>(SceneArenaSize);
        g_streamingArena = new TArena<StreamingAllocator>(StreamingArenaSize);

        g_inputManager = CreateObject<InputManager>();

        g_logger = CreateObject<Logger>();
        g_logger->fatalErrorHook = &HandleFatalError;

        InitObject(g_logger);

        LogChannelRegistrar::GetInstance().RegisterAll();

        NameRegistry_Initialize();

        ClassRegistry::GetInstance().Initialize();
        HypScript::GetInstance().Initialize();

        const FilePath basePath = FilePath(CoreApi_GetCommandLineArguments().GetCommand()).BasePath();
        CoreApi_SetExecutablePath(basePath);

        const bool isEditor = CoreApi_GetCommandLineArguments()["Editor"].ToBool();

        // dont initialize hostfxr if running from editor,
        // leads to type identity issues with managed types
        // due to multiple runtimes being loaded.
        DotNETHost::GetInstance().Initialize(basePath, /* initFromManaged */ isEditor, s_initFromManagedCallback);

        ConsoleCommandManager::GetInstance().Initialize();
        AudioManager::GetInstance().Initialize();
        TaskSystem::GetInstance().Start();

        ConfigurationTable renderGlobalConfigOverrides;

        g_engineDriver = CreateObject<EngineDriver>();

        g_engineStats = CreateObject<EngineStats>();
        InitObject(g_engineStats);

        g_streamingManager = CreateObject<StreamingManager>();
        InitObject(g_streamingManager);
        g_streamingManager->Start();

        g_assetManager = CreateObject<AssetManager>();
        InitObject(g_assetManager);

#ifdef HYP_EDITOR
        g_editorState = CreateObject<EditorState>();
        InitObject(g_editorState);
#endif

        g_shaderManager = new ShaderManager;
        g_materialSystem = new MaterialCache;
        g_safeDeleter = new SafeDeleter;

        g_shaderCompiler = new ShaderCompiler;
        if (!g_shaderCompiler->LoadShaderDefinitions())
        {
            HYP_LOG(Engine, Error, "Failed to load shader definitions!");
        }

        ComponentInterfaceRegistry::GetInstance().Initialize();

#ifdef HYP_LIBUI
        uiInitOptions options = {};
        const char* err = uiInit(&options);
        if (err != nullptr)
        {
            uiFreeInitError(err);

            HYP_FAIL("Failed to initialize libui! Message: {}", err);

            return;
        }
#endif

        const CommandLineArguments& cliArgs = CoreApi_GetCommandLineArguments();

#ifdef HYP_WINDOWS
        g_appContext = CreateObject<Win32AppContext>("Hyperion", cliArgs);
#elif defined(HYP_MACOS)
        g_appContext = CreateObject<CocoaAppContext>("Hyperion", cliArgs);
#elif defined(HYP_SDL)
        g_appContext = CreateObject<SDLAppContext>("Hyperion", cliArgs);
#else
        HYP_FAIL("AppContext not implemented for this platform");
#endif

        Vec2i resolution = { 1280, 720 };

        EnumFlags<WindowFlags> windowFlags = WindowFlags::HIGH_DPI | WindowFlags::EVENTS_POLLING;

        if (cliArgs["Headless"].ToBool())
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

            g_appContext->SetMainWindow(g_appContext->CreateSystemWindow({ "Hyperion Engine", resolution, windowFlags }));
        }
        else
        {
            HYP_LOG(Engine, Info, "Running in headless mode");
        }

        g_mainThreadInstance = new MainThread();
        g_renderThreadInstance = new RenderThread();
        g_gameThreadInstance = new GameThread();

        InitObject(g_engineDriver);

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

        g_gameThreadInstance->Join();

        g_renderThreadInstance->Join();
        g_renderThread = g_mainThread;

        g_engineDriver->FinalizeStop();

#ifdef HYP_LIBUI
        uiUninit();
#endif

        RenderApi::Shutdown();

        DotNETHost::GetInstance().Shutdown();
        ComponentInterfaceRegistry::GetInstance().Shutdown();
        ConsoleCommandManager::GetInstance().Shutdown();
        AudioManager::GetInstance().Shutdown();

        if (TaskSystem::GetInstance().IsRunning())
        {
            TaskSystem::GetInstance().Stop();
        }

        NameRegistry_Shutdown();

        CoreApi_Shutdown();

        g_streamingManager->Stop();
        g_streamingManager.Reset();

        g_assetManager.Reset();
        g_editorState.Reset();
        g_engineStats.Reset();

        delete g_shaderCompiler;
        g_shaderCompiler = nullptr;

        delete g_shaderManager;
        g_shaderManager = nullptr;

        delete g_materialSystem;
        g_materialSystem = nullptr;

        g_engineDriver.Reset();

        delete g_renderBackend;
        g_renderBackend = nullptr;

        delete g_renderArena;
        g_renderArena = nullptr;

        delete g_sceneArena;
        g_sceneArena = nullptr;

        delete g_streamingArena;
        g_streamingArena = nullptr;

        delete g_scenePool;
        g_scenePool = nullptr;

        delete g_streamingPool;
        g_streamingPool = nullptr;

        delete g_scriptPool;
        g_scriptPool = nullptr;

        delete g_taskPool;
        g_taskPool = nullptr;

        delete g_resourcePool;
        g_resourcePool = nullptr;

        delete g_assetPool;
        g_assetPool = nullptr;

        for (uint32 i = 0; i < RingBufferDepth; i++)
        {
            delete g_framePools[i];
            g_framePools[i] = nullptr;
        }

        delete g_renderPool;
        g_renderPool = nullptr;

        delete g_objectPool;
        g_objectPool = nullptr;

        delete g_safeDeleter;
        g_safeDeleter = nullptr;

        delete g_mainThreadInstance;
        g_mainThreadInstance = nullptr;

        delete g_gameThreadInstance;
        g_gameThreadInstance = nullptr;

        delete g_renderThreadInstance;
        g_renderThreadInstance = nullptr;

#ifdef HYP_WINDOWS
        sys::Win32_CleanupWindowClasses();
#endif
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

#ifdef HYP_MACOS
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

        HypData hd;
        if (!pGameClass->CreateInstance(hd) || !hd.Is<Game>())
        {
            HYP_LOG(Engine, Error, "Failed to create game: could not create instance of class '{}'", gameClassName);
            return nullptr;
        }

        Handle<Game>& gameHandle = hd.Get<Handle<Game>>();
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

    HYP_EXPORT int Hyp_LaunchGame(Game* pGame)
    {
        if (!pGame)
        {
            return 0;
        }

        App::GetInstance().LaunchGame(MakeStrongRef(pGame));

        return 1;
    }

    HYP_EXPORT void Hyp_MainThreadUpdate()
    {
        AssertOnThread(g_mainThread);

        g_mainThreadInstance->Update();
    }

    HYP_EXPORT void Hyp_SetInitFromManagedCallback(void (*callback)())
    {
        s_initFromManagedCallback = callback;
    }

#ifdef HYP_EDITOR
    using LogCallback = void (*)(const char* channel, int level, double timestamp, const char* message);

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

            g_logCallback(channel.name.LookupString(), (int)message.level, (double)message.timestamp, text.Data());
        }

        // allow default logging to continue
        return true;
    }

    HYP_EXPORT void Editor_RegisterLogCallback(LogCallback callback)
    {
        g_logCallback = callback;

        if (g_logRedirectId == -1)
        {
            //g_logRedirectId = g_logger->GetOutputStream()->AddRedirect(
            //    Bitset(~0u), // All channels
            //    nullptr,
            //    HandleLogMessage,
            //    HandleLogMessage // Use same handler for errors for now
            //);
        }
    }

    HYP_EXPORT void Editor_ExecuteConsoleCommand(const char* command)
    {
        if (!command)
            return;

        ConsoleCommandManager::GetInstance().ExecuteCommand(command);
    }
#endif
}

} // namespace hyperion
