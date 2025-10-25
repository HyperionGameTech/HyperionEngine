/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionEngine.hpp>

#include <asset/Assets.hpp>

#include <dotnet/DotNetSystem.hpp>

#include <core/Core.hpp>

#include <core/reflection/HypClassRegistry.hpp>

#include <core/reflection/TypeInfo.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/logging/Logger.hpp>

#include <core/cli/CommandLine.hpp>

#include <console/ConsoleCommandManager.hpp>

#include <system/MessageBox.hpp>

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

#include <core/reflection/Handle.hpp>

#include <script/HypScript.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineMemory.hpp>
#include <engine/EngineStats.hpp>

#include <game/Game.hpp>

/// ========== If this include is missing, you need to run HypBuildTool (instructions in doc/CompilingTheEngine.md) ==========
#include <BuildToolOutput.inc>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Engine);

#pragma region Memory Pools

static constexpr SizeType ObjectPoolBlockSize = 16 * 1024 * 1024;
static constexpr SizeType RenderPoolBlockSize = 16 * 1024 * 1024;
static constexpr SizeType FramePoolBlockSize = 4 * 1024 * 1024;
static constexpr SizeType ScenePoolBlockSize = 8 * 1024 * 1024;
static constexpr SizeType TaskPoolBlockSize = 4 * 1024 * 1024;

HYP_API Pool* g_objectPool;
HYP_API Pool* g_renderPool;
HYP_API Pool* g_framePools[NumMultiBuffers];
HYP_API Pool* g_scenePool;
HYP_API Pool* g_taskPool;

Pool* const* g_enginePools[EPN_MAX] = {
    &g_objectPool, // EPN_CORE
    &g_renderPool, // EPN_RENDER
    &g_scenePool   // EPN_SCENE
};

HYP_API Pool* GetCurrentFramePool()
{
    const uint32 currentFrameIndex = RenderApi::GetFrameIndex();

    return g_framePools[currentFrameIndex];
}

#pragma endregion Memory Pools

Handle<EngineDriver> g_engineDriver;
Handle<AssetManager> g_assetManager;
Handle<EditorState> g_editorState;
Handle<AppContextBase> g_appContext;
Handle<Logger> g_logger;
ShaderManager* g_shaderManager;
MaterialCache* g_materialSystem;
SafeDeleter* g_safeDeleter;
IRenderBackend* g_renderBackend;
RenderGlobalState* g_renderGlobalState;
ShaderCompiler* g_shaderCompiler;

static void HandleFatalError(const char* message)
{
    SystemMessageBox(MessageBoxType::CRITICAL)
        .Title("Fatal error logged!")
        .Text(message)
        .Show();

    std::terminate();
}

template <auto DirectoryStaticString>
struct DirectoryInitializer
{
    FilePath path;

    DirectoryInitializer()
    {
#if defined(HYP_DEBUG_MODE) && defined(HYP_ROOT_DIR)
        path = FilePath(HYP_ROOT_DIR) / DirectoryStaticString.Data();
#else
        path = CoreApi_GetExecutablePath() / DirectoryStaticString.Data();
#endif

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

HYP_API const FilePath& GetResourceDirectory()
{
    static DirectoryInitializer<HYP_STATIC_STRING("res")> s_resourceDirectory;
    return s_resourceDirectory.path;
}

HYP_API bool InitializeEngine(int argc, char** argv)
{
    Threads::SetCurrentThreadId(g_mainThread);

    g_objectPool = new Pool(ObjectPoolBlockSize, PF_NONE);
    g_renderPool = new Pool(RenderPoolBlockSize, PF_NONE, g_renderThread);

    for (uint32 i = 0; i < NumMultiBuffers; i++)
    {
        g_framePools[i] = new Pool(FramePoolBlockSize, PF_NONE);
    }

    g_scenePool = new Pool(ScenePoolBlockSize, PF_THREAD_SAFE);
    g_taskPool = new Pool(TaskPoolBlockSize, PF_THREAD_SAFE);

    g_logger = CreateObject<Logger>();
    g_logger->fatalErrorHook = &HandleFatalError;

    InitObject(g_logger);

    LogChannelRegistrar::GetInstance().RegisterAll();

    NameRegistry_Initialize();

    HypClassRegistry::GetInstance().Initialize();
    HypScript::GetInstance().Initialize();

    if (!CoreApi_Initialize(argc, argv))
    {
        return false;
    }

    EngineStats_Initialize();

    const FilePath basePath = FilePath(CoreApi_GetCommandLineArguments().GetCommand()).BasePath();
    CoreApi_SetExecutablePath(basePath);

    dotnet::DotNetSystem::GetInstance().Initialize(basePath);
    ConsoleCommandManager::GetInstance().Initialize();
    AudioManager::GetInstance().Initialize();
    TaskSystem::GetInstance().Start();

#ifdef HYP_VULKAN
    g_renderBackend = new VulkanRenderBackend();
#else
#error Unsupported rendering backend
#endif

    ConfigurationTable renderGlobalConfigOverrides;

    g_engineDriver = CreateObject<EngineDriver>();

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

    const CommandLineArguments& cliArgs = CoreApi_GetCommandLineArguments();

#ifdef HYP_WINDOWS
    g_appContext = CreateObject<Win32AppContext>("Hyperion", cliArgs);
#elif defined(HYP_SDL)
    g_appContext = CreateObject<SDLAppContext>("Hyperion", cliArgs);
#else
    HYP_FAIL("AppContext not implemented for this platform");
#endif

    Vec2i resolution = { 1280, 720 };

    EnumFlags<WindowFlags> windowFlags = WindowFlags::HIGH_DPI;

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

    RenderApi::Init();

    InitObject(g_engineDriver);

    return true;
}

HYP_API void DestroyEngine()
{
    Threads::AssertOnThread(g_mainThread);

    Assert(
        g_engineDriver != nullptr,
        "Hyperion not initialized!");

    g_engineDriver->FinalizeStop();

    dotnet::DotNetSystem::GetInstance().Shutdown();
    ComponentInterfaceRegistry::GetInstance().Shutdown();
    ConsoleCommandManager::GetInstance().Shutdown();
    AudioManager::GetInstance().Shutdown();

    if (TaskSystem::GetInstance().IsRunning())
    {
        TaskSystem::GetInstance().Stop();
    }

    EngineStats_Shutdown();

    NameRegistry_Shutdown();

    CoreApi_Shutdown();

    g_assetManager.Reset();
    g_editorState.Reset();

    delete g_shaderCompiler;
    g_shaderCompiler = nullptr;

    delete g_shaderManager;
    g_shaderManager = nullptr;

    delete g_materialSystem;
    g_materialSystem = nullptr;

    g_engineDriver.Reset();

    delete g_renderBackend;
    g_renderBackend = nullptr;

    delete g_scenePool;
    g_scenePool = nullptr;

    delete g_taskPool;
    g_taskPool = nullptr;

    for (uint32 i = 0; i < NumMultiBuffers; i++)
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
}

} // namespace hyperion
