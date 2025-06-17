/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionEngine.hpp>

#include <asset/Assets.hpp>

#include <dotnet/DotNetSystem.hpp>

#include <scene/Material.hpp>

#include <scene/ecs/ComponentInterface.hpp>

#include <core/object/HypClassRegistry.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>

#include <core/cli/CommandLine.hpp>

#include <console/ConsoleCommandManager.hpp>

#include <system/MessageBox.hpp>

#include <streaming/StreamingManager.hpp>

#include <rendering/SafeDeleter.hpp>

#ifdef HYP_VULKAN
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>
#endif

#include <audio/AudioManager.hpp>

#include <core/Handle.hpp>

#include <Engine.hpp>
#include <Game.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

static CommandLineArguments g_command_line_arguments;

static bool InitializeCommandLineArguments(int argc, char** argv)
{
    CommandLineParser arg_parse { &DefaultCommandLineArgumentDefinitions() };

    TResult<CommandLineArguments> parse_result = arg_parse.Parse(argc, argv);

    if (parse_result.HasError())
    {
        const Error& error = parse_result.GetError();

        HYP_LOG(Core, Error, "Failed to parse command line arguments!\n\t{}", error.GetMessage().Any() ? error.GetMessage() : "<no message>");

        g_command_line_arguments = CommandLineArguments();

        return false;
    }

    GlobalConfig config { "app" };

    if (json::JSONValue config_args = config.Get("app.args"))
    {
        json::JSONString config_args_string = config_args.ToString();
        Array<String> config_args_string_split = config_args_string.Split(' ');

        parse_result = arg_parse.Parse(g_command_line_arguments.GetCommand(), config_args_string_split);

        if (parse_result.HasError())
        {
            HYP_LOG(Core, Error, "Failed to parse config command line value \"{}\":\n\t{}", config_args_string, parse_result.GetError().GetMessage());

            return false;
        }

        g_command_line_arguments = CommandLineArguments::Merge(*arg_parse.GetDefinitions(), *parse_result, g_command_line_arguments);
    }

    return true;
}

HYP_API const CommandLineArguments& GetCommandLineArguments()
{
    return g_command_line_arguments;
}

HYP_API const FilePath& GetExecutablePath()
{
    static FilePath executable_path = FilePath(g_command_line_arguments.GetCommand()).BasePath();

    return executable_path;
}

HYP_API const FilePath& GetResourceDirectory()
{
    static struct ResourceDirectoryData
    {
        FilePath path;

        ResourceDirectoryData()
        {
#ifdef HYP_DEBUG_MODE
            path = FilePath(HYP_ROOT_DIR) / "res";
#else
            path = GetExecutablePath() / "res";
#endif
            AssertThrowMsg(path.Exists() && path.IsDirectory(), "Resource directory does not exist or is not a directory: %s", path.Data());

            AssertThrowMsg(path.CanRead(), "Resource directory is not readable: %s", path.Data());
            AssertThrowMsg(path.CanWrite(), "Resource directory is not writable: %s", path.Data());
        }
    } resource_directory_data;

    return resource_directory_data.path;
}

HYP_API bool InitializeEngine(int argc, char** argv)
{
    Threads::SetCurrentThreadID(g_main_thread);

    HypClassRegistry::GetInstance().Initialize();

    if (!InitializeCommandLineArguments(argc, argv))
    {
        return false;
    }

    const FilePath base_path = GetExecutablePath();

    dotnet::DotNetSystem::GetInstance().Initialize(base_path);
    ConsoleCommandManager::GetInstance().Initialize();
    AudioManager::GetInstance().Initialize();

    g_engine = CreateObject<Engine>();

    g_asset_manager = CreateObject<AssetManager>();
    InitObject(g_asset_manager);

    g_shader_manager = new ShaderManager;
    g_material_system = new MaterialCache;
    g_safe_deleter = new SafeDeleter;

#ifdef HYP_VULKAN
    g_rendering_api = new renderer::VulkanRenderingAPI();
#else
#error Unsupported rendering backend
#endif

    ComponentInterfaceRegistry::GetInstance().Initialize();

    return true;
}

HYP_API void DestroyEngine()
{
    Threads::AssertOnThread(g_main_thread);

    AssertThrowMsg(
        g_engine != nullptr,
        "Hyperion not initialized!");

    g_engine->FinalizeStop();

    dotnet::DotNetSystem::GetInstance().Shutdown();
    ComponentInterfaceRegistry::GetInstance().Shutdown();
    ConsoleCommandManager::GetInstance().Shutdown();
    AudioManager::GetInstance().Shutdown();

    g_asset_manager.Reset();

    delete g_shader_manager;
    g_shader_manager = nullptr;

    delete g_material_system;
    g_material_system = nullptr;

    g_engine.Reset();

    delete g_rendering_api;
    g_rendering_api = nullptr;
}

} // namespace hyperion