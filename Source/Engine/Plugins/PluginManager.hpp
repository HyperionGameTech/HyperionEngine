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

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Name/Name.hpp>

#include <Engine/Plugins/PluginAPI.hpp>
#include <Engine/Plugins/PluginManifest.hpp>
#include <Engine/Plugins/PluginDescriptor.hpp>
#include <Engine/Plugins/PluginModule.hpp>
#include <Engine/Plugins/PluginContext.hpp>

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

    HYP_FORCE_INLINE const Array<PluginDescriptor>& GetBuiltinPlugins() const
    {
        return m_builtinPlugins;
    }

    const PluginDescriptor* FindBuiltinPlugin(Name pluginName) const;

    Array<Name> GetDefaultEnabledPlugins() const;

    HYP_FORCE_INLINE const Array<Name>& GetEnabledPlugins() const
    {
        return m_enabledPlugins;
    }

    bool IsPluginEnabled(Name pluginName) const;

    // the modules a game links for these plugins and their dependencies (no *Editor modules)
    Array<Name> GetRuntimeModules(const Array<Name>& pluginNames) const;

    void SetEnabledPlugins(const Array<Name>& pluginNames);

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

    struct ActiveModule
    {
        Name pluginName;
        Name moduleName;
        UniquePtr<IPluginModule> module;
        UniquePtr<PluginContext> context;
    };

    void LoadPluginFromDirectory(const FilePath& directory);

    void DiscoverBuiltinPlugins();
    void ResolveEnabledPlugin(Name pluginName, Array<Name>& outOrdered, Array<Name>& visiting) const;
    void UnloadModule(ActiveModule& activeModule);

    bool m_initialized = false;

    PluginHostMode m_hostMode = PluginHostMode::Game;
    Array<LoadedPlugin> m_plugins;

    Array<PluginDescriptor> m_builtinPlugins;
    Array<Name> m_enabledPlugins;
    Array<ActiveModule> m_activeModules;
};

} // namespace Hyperion
