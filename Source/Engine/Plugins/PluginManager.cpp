/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Engine/Plugins/PluginManager.hpp>

#include <Core/Core.hpp>

#include <Core/DLL/DynamicLibrary.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <algorithm>

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(Plugins);

namespace {

class PluginHostImpl final : public IPluginHost
{
public:
    uint32 GetABIVersion() const override
    {
        return HYP_PLUGIN_ABI_VERSION;
    }

    uint32 GetEngineVersionMajor() const override
    {
        return HYP_VERSION_MAJOR;
    }

    uint32 GetEngineVersionMinor() const override
    {
        return HYP_VERSION_MINOR;
    }

    uint32 GetEngineVersionPatch() const override
    {
        return HYP_VERSION_PATCH;
    }

    bool IsEditorHost() const override
    {
        return m_hostMode == PluginHostMode::Editor;
    }

    void SetHostMode(PluginHostMode hostMode)
    {
        m_hostMode = hostMode;
    }

private:
    PluginHostMode m_hostMode = PluginHostMode::Game;
};

using PluginQueryFn = HypPluginDescriptor* (*)();
using PluginLoadFn = IPlugin* (*)(const IPluginHost*);
using PluginUnloadFn = void (*)(IPlugin*);

static PluginHostImpl s_pluginHost;

bool ParseVersionString(const String& versionString, uint32& outMajor, uint32& outMinor, uint32& outPatch)
{
    const Array<String> parts = versionString.Split('.');

    if (parts.Size() != 3)
    {
        return false;
    }

    outMajor = StringUtil::Parse(parts[0], 0u);
    outMinor = StringUtil::Parse(parts[1], 0u);
    outPatch = StringUtil::Parse(parts[2], 0u);

    return true;
}

} // namespace

PluginManager& PluginManager::GetInstance()
{
    static PluginManager s_instance;

    return s_instance;
}

void PluginManager::Initialize(PluginHostMode hostMode)
{
    if (m_initialized)
    {
        HYP_LOG(Plugins, Warning, "PluginManager::Initialize() called more than once - ignoring");

        return;
    }

    m_hostMode = hostMode;
    s_pluginHost.SetHostMode(hostMode);

    const FilePath pluginsDir = CoreApi::GetExecutablePath() / "Plugins";

    if (!pluginsDir.Exists() || !pluginsDir.IsDirectory())
    {
        HYP_LOG(Plugins, Info, "No Plugins directory at {} - no plugins loaded", pluginsDir);

        m_initialized = true;

        return;
    }

    Array<FilePath> pluginDirectories = pluginsDir.GetSubdirectories();
    std::sort(pluginDirectories.Begin(), pluginDirectories.End());

    for (const FilePath& pluginDirectory : pluginDirectories)
    {
        if (!pluginDirectory.IsDirectory())
        {
            continue;
        }

        LoadPluginFromDirectory(pluginDirectory);
    }

    m_initialized = true;

    HYP_LOG(Plugins, Info, "Plugin manager initialized (host: {}, {} plugin(s) loaded)",
        m_hostMode == PluginHostMode::Editor ? "editor" : "game",
        m_plugins.Size());
}

void PluginManager::OnEditorLaunch()
{
    for (LoadedPlugin& loadedPlugin : m_plugins)
    {
        loadedPlugin.plugin->OnEditorLaunch();
    }
}

void PluginManager::OnEditorShutdown()
{
    for (LoadedPlugin& loadedPlugin : m_plugins)
    {
        loadedPlugin.plugin->OnEditorShutdown();
    }
}

void PluginManager::Shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    // Reverse load order so plugins that were loaded after another are torn down first.
    for (size_t index = m_plugins.Size(); index != 0; index--)
    {
        LoadedPlugin& loadedPlugin = m_plugins[index - 1];

        loadedPlugin.plugin->OnShutdown();
        loadedPlugin.unloadFn(loadedPlugin.plugin);
        loadedPlugin.plugin = nullptr;

        HYP_LOG(Plugins, Info, "Unloaded plugin '{}'", loadedPlugin.manifest.name);
    }

    m_plugins.Clear();

    // DynamicLibrary objects are owned by the DynamicLibraryCache and are intentionally
    // left loaded - unloading DLLs whose code may still be referenced (task threads,
    // static destructors) is not safe at this point in shutdown.

    m_initialized = false;
}

void PluginManager::LoadPluginFromDirectory(const FilePath& directory)
{
    const EnumFlags<HypPluginHostFlags> hostFlag = (m_hostMode == PluginHostMode::Editor)
        ? HypPluginHostFlags::Editor
        : HypPluginHostFlags::Game;

    const FilePath manifestPath = directory / "plugin.json";

    TResult<PluginManifest> manifestResult = PluginManifest::Parse(manifestPath);

    if (manifestResult.HasError())
    {
        HYP_LOG(Plugins, Warning, "Skipping plugin directory {}: {}", directory, manifestResult.GetError().GetMessage());

        return;
    }

    PluginManifest manifest = manifestResult.GetValue();

    if (!(manifest.hostFlags & hostFlag))
    {
        HYP_LOG(Plugins, Verbose, "Skipping plugin '{}' (host not supported by manifest)", manifest.name);

        return;
    }

    for (const LoadedPlugin& loadedPlugin : m_plugins)
    {
        if (loadedPlugin.manifest.name == manifest.name)
        {
            HYP_LOG(Plugins, Warning, "Skipping plugin '{}' from {} - a plugin with this name is already loaded",
                manifest.name,
                directory);

            return;
        }
    }

    if (!manifest.engineMinVersion.Empty())
    {
        uint32 minMajor = 0;
        uint32 minMinor = 0;
        uint32 minPatch = 0;

        if (!ParseVersionString(manifest.engineMinVersion, minMajor, minMinor, minPatch)
            || HYP_VERSION_MAJOR < minMajor
            || (HYP_VERSION_MAJOR == minMajor && HYP_VERSION_MINOR < minMinor)
            || (HYP_VERSION_MAJOR == minMajor && HYP_VERSION_MINOR == minMinor && HYP_VERSION_PATCH < minPatch))
        {
            HYP_LOG(Plugins, Warning,
                "Skipping plugin '{}': requires engine version >= {} but host is {}.{}.{}",
                manifest.name,
                manifest.engineMinVersion,
                HYP_VERSION_MAJOR,
                HYP_VERSION_MINOR,
                HYP_VERSION_PATCH);

            return;
        }
    }

    const FilePath libraryPath = directory / manifest.entry;

    if (!libraryPath.Exists())
    {
        HYP_LOG(Plugins, Warning, "Skipping plugin '{}': entry library does not exist at {}", manifest.name, libraryPath);

        return;
    }

    DynamicLibrary* library = DynamicLibraryCache::GetInstance().LoadLibrary(PlatformString(String(libraryPath)));

    if (library == nullptr)
    {
        HYP_LOG(Plugins, Error, "Failed to load plugin '{}' library at {}", manifest.name, libraryPath);

        return;
    }

    const PluginQueryFn queryFn = reinterpret_cast<PluginQueryFn>(library->GetFunction(PluginFunctionName_Query));
    const PluginLoadFn loadFn = reinterpret_cast<PluginLoadFn>(library->GetFunction(PluginFunctionName_Load));
    const PluginUnloadFn unloadFn = reinterpret_cast<PluginUnloadFn>(library->GetFunction(PluginFunctionName_Unload));

    if (queryFn == nullptr || loadFn == nullptr || unloadFn == nullptr)
    {
        HYP_LOG(Plugins, Error,
            "Library at {} is missing plugin entry points",
            libraryPath);

        return;
    }

    HypPluginDescriptor* descriptor = queryFn();

    if (descriptor == nullptr || descriptor->name == nullptr)
    {
        HYP_LOG(Plugins, Error, "Plugin library at {} returned no descriptor from HypPluginQuery()", libraryPath);

        return;
    }

    if (descriptor->abiVersion != HYP_PLUGIN_ABI_VERSION)
    {
        HYP_LOG(Plugins, Error,
            "Plugin '{}' was built against plugin ABI version {} but the host uses ABI version {} - rebuild the plugin against this engine version",
            descriptor->name,
            descriptor->abiVersion,
            HYP_PLUGIN_ABI_VERSION);

        return;
    }

    IPlugin* plugin = loadFn(&s_pluginHost);

    if (plugin == nullptr)
    {
        HYP_LOG(Plugins, Error, "Plugin '{}' failed to initialize", descriptor->name);

        return;
    }

    plugin->OnInitialize(&s_pluginHost);

    LoadedPlugin loadedPlugin;
    loadedPlugin.manifest = std::move(manifest);
    loadedPlugin.directory = directory;
    loadedPlugin.library = library;
    loadedPlugin.plugin = plugin;
    loadedPlugin.unloadFn = unloadFn;

    m_plugins.PushBack(std::move(loadedPlugin));

    LoadedPlugin& storedPlugin = m_plugins.Back();

    HYP_LOG(Plugins, Info, "Loaded plugin '{}' v{} from {}",
        storedPlugin.manifest.name,
        storedPlugin.manifest.version,
        directory);
}

} // namespace Hyperion
