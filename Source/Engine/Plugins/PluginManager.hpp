/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Types.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Logging/Logger.hpp>

#include <Engine/Plugins/PluginAPI.hpp>
#include <Engine/Plugins/PluginManifest.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Plugins);

class DynamicLibrary;

enum class PluginHostMode : uint32
{
    Editor,
    Game
};

class ENGINE_API PluginManager
{
public:
    static PluginManager& GetInstance();

    PluginManager(const PluginManager& other) = delete;
    PluginManager& operator=(const PluginManager& other) = delete;

    void Initialize(PluginHostMode hostMode);
    void Shutdown();

    bool IsInitialized() const
    {
        return m_initialized;
    }

    void OnEditorLaunch();
    void OnEditorShutdown();

    uint32 NumLoadedPlugins() const
    {
        return uint32(m_plugins.Size());
    }

private:
    PluginManager() = default;
    ~PluginManager() = default;

    struct LoadedPlugin
    {
        PluginManifest manifest;
        FilePath directory;
        DynamicLibrary* library = nullptr;
        IPlugin* plugin = nullptr;

        using UnloadFn = void (*)(IPlugin*);
        UnloadFn unloadFn = nullptr;
    };

    void LoadPluginFromDirectory(const FilePath& directory);

    bool m_initialized = false;

    PluginHostMode m_hostMode = PluginHostMode::Game;
    Array<LoadedPlugin> m_plugins;
};

} // namespace Hyperion
