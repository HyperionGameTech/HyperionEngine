/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionEngine.hpp>

#include <asset/Assets.hpp>

#include <dotnet/DotNetSystem.hpp>

#include <scene/Material.hpp>

#include <scene/ecs/ComponentInterface.hpp>

#include <core/object/HypClassRegistry.hpp>
#include <core/threading/Threads.hpp>

#include <console/ConsoleCommandManager.hpp>

#include <system/MessageBox.hpp>

#include <streaming/StreamingManager.hpp>

#include <rendering/SafeDeleter.hpp>

#ifdef HYP_VULKAN
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>
#endif

#include <audio/AudioManager.hpp>

#include <core/Handle.hpp>

#include <Game.hpp>

namespace hyperion {

HYP_API void InitializeAppContext(const Handle<AppContextBase>& app_context, Game* game)
{
    Threads::AssertOnThread(g_main_thread);

    AssertThrow(app_context != nullptr);

    AssertThrowMsg(g_engine.IsValid(), "Engine is null!");
    AssertThrowMsg(game != nullptr, "Game is null!");

    game->SetAppContext(app_context);
    g_engine->SetAppContext(app_context);

    InitObject(g_engine);
}

HYP_API void InitializeEngine(const FilePath& base_path)
{
    Threads::SetCurrentThreadID(g_main_thread);

    HypClassRegistry::GetInstance().Initialize();
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