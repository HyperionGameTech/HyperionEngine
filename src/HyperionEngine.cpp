/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionEngine.hpp>

#include <asset/Assets.hpp>

#include <dotnet/DotNetSystem.hpp>

#include <core/Core.hpp>

#include <core/object/HypClassRegistry.hpp>

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

#include <core/object/Handle.hpp>

#include <script/HypScript.hpp>

#include <engine/EngineDriver.hpp>
#include <game/Game.hpp>

/// ========== If this include is missing, you need to run HypBuildTool (instructions in doc/CompilingTheEngine.md) ==========
#include <BuildToolOutput.inc>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Engine);

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
    g_logger = CreateObject<Logger>();
    g_logger->fatalErrorHook = &HandleFatalError;

    InitObject(g_logger);

    LogChannelRegistrar::GetInstance().RegisterAll();

    Threads::SetCurrentThreadId(g_mainThread);

    InitializeNameRegistry();

    HypClassRegistry::GetInstance().Initialize();
    HypScript::GetInstance().Initialize();

    if (!CoreApi_InitializeCommandLineArguments(argc, argv))
    {
        return false;
    }

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

    RenderApi_Init();

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
}

} // namespace hyperion
