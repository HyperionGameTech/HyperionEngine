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

#include <Engine/Plugins/PluginDescriptor.hpp>
#include <Engine/Plugins/PluginModule.hpp>
#include <Engine/Plugins/PluginContext.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Plugins);

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

    HYP_FORCE_INLINE const Array<PluginDescriptor>& GetPlugins() const
    {
        return m_plugins;
    }

    const PluginDescriptor* FindPlugin(StringHash pluginName) const;

    /// the Content folders of these plugins and the ones they depend on, for a process that reads their assets
    /// without loading them (the cook)
    Array<FilePath> GetPluginContentDirectories(Span<const StringHash> pluginNames);

    Array<Name> GetDefaultEnabledPlugins() const;

    HYP_FORCE_INLINE const Array<Name>& GetEnabledPlugins() const
    {
        return m_enabledPlugins;
    }

    bool IsPluginEnabled(Name pluginName) const;

    /// the modules a game links for these plugins and their dependencies (no *Editor modules)
    Array<Name> GetRuntimeModules(Span<const Name> pluginNames) const;

    void SetEnabledPlugins(Span<const Name> pluginNames);

    /// loads the libraries and C# of these plugins and their dependencies without starting their modules
    void PreloadPlugins(Span<const Name> pluginNames);

    bool ArePluginLibrariesLoaded(Name pluginName) const;
    void LoadAllPluginLibraries();

private:
    PluginManager() = default;
    ~PluginManager() = default;

    struct ActiveModule
    {
        Name pluginName;
        Name moduleName;
        UniquePtr<IPluginModule> module;
        UniquePtr<PluginContext> context;
    };

    void DiscoverPlugins();
    void LoadPluginCode(const PluginDescriptor& descriptor);
    void LoadPluginLibraries(const PluginDescriptor& descriptor);
    void ResolveEnabledPlugin(StringHash pluginName, Array<Name>& outOrdered, Array<Name>& visiting) const;
    void UnloadModule(ActiveModule& activeModule);

    // a plugin's Content folder joins the engine registry while the plugin is enabled
    void SetPluginContentMounted(Name pluginName, bool mounted) const;

    bool m_initialized = false;

    PluginHostMode m_hostMode = PluginHostMode::Game;
    Array<PluginDescriptor> m_plugins;
    Array<Name> m_enabledPlugins;
    Array<ActiveModule> m_activeModules;
    Array<Name> m_pluginsWithLibrariesLoaded;
    Array<Name> m_modulesAnnouncedToManaged;
};

} // namespace Hyperion
