/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Engine/Plugins/PluginManager.hpp>

#include <Core/Core.hpp>

#include <Core/DLL/DynamicLibrary.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>
#include <Core/DataProcessing/Shared/CompilerError.hpp>
#include <Core/DataProcessing/Shared/ErrorList.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <algorithm>

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(Plugins);

static Array<PluginModuleRegistration>& GetPluginModuleRegistrations()
{
    static Array<PluginModuleRegistration> s_registrations;

    return s_registrations;
}

void RegisterPluginModule(const PluginModuleRegistration& registration)
{
    GetPluginModuleRegistrations().PushBack(registration);
}

const Array<PluginModuleRegistration>& GetRegisteredPluginModules()
{
    return GetPluginModuleRegistrations();
}

static Array<PluginModuleInitializer>& GetPluginModuleInitializers()
{
    static Array<PluginModuleInitializer> s_initializers;

    return s_initializers;
}

void AddPluginModuleInitializer(PluginModuleInitializer initializer)
{
    GetPluginModuleInitializers().PushBack(initializer);
}

void InitializePluginModules()
{
    for (PluginModuleInitializer initializer : GetPluginModuleInitializers())
    {
        initializer();
    }
}

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

    DiscoverBuiltinPlugins();

    // the editor enables plugins per project when one opens
    if (m_hostMode == PluginHostMode::Game)
    {
        SetEnabledPlugins(GetDefaultEnabledPlugins());
    }

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

    SetEnabledPlugins({});

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

void PluginManager::DiscoverBuiltinPlugins()
{
    m_builtinPlugins.Clear();

    const FilePath& baseDirectory = CoreApi::GetBaseDirectory();
    const FilePath searchDirectories[] = { baseDirectory / "Plugins", baseDirectory / "Tests" / "Plugins" };

    for (const PluginModuleRegistration& registration : GetRegisteredPluginModules())
    {
        const Name pluginName = CreateNameFromDynamicString(registration.pluginName);

        if (FindBuiltinPlugin(pluginName) != nullptr)
        {
            continue;
        }

        // without a descriptor (a packaged game) the build only links the plugins the game uses
        PluginDescriptor descriptor;
        descriptor.enabledByDefault = true;

        for (const FilePath& searchDirectory : searchDirectories)
        {
            const FilePath pluginDirectory = searchDirectory / pluginName.LookupString();
            const FilePath descriptorPath = pluginDirectory / (String(pluginName.LookupString()) + ".hypplugin");

            if (!descriptorPath.Exists())
            {
                continue;
            }

            FileByteReader stream { descriptorPath };

            HMF::ErrorList errorList;
            HMF::ParseResult parseResult = HMF::Parse(descriptorPath, stream, &errorList);

            if (parseResult.HasError() || !parseResult.GetValue().Is<PluginDescriptor>())
            {
                String parseMessages;
                errorList.WriteAllMessages(parseMessages);

                HYP_LOG(Plugins, Warning, "Failed to read plugin descriptor {}: {}", descriptorPath, parseMessages);

                break;
            }

            descriptor = parseResult.GetValue().Get<PluginDescriptor>();
            descriptor.directory = pluginDirectory;

            break;
        }

        descriptor.name = pluginName;

        HYP_LOG(Plugins, Info, "Found built-in plugin '{}' v{}", pluginName, descriptor.version);

        m_builtinPlugins.PushBack(std::move(descriptor));
    }
}

const PluginDescriptor* PluginManager::FindBuiltinPlugin(Name pluginName) const
{
    auto it = m_builtinPlugins.FindIf([pluginName](const PluginDescriptor& descriptor)
        {
            return descriptor.name == pluginName;
        });

    return it != m_builtinPlugins.End() ? &*it : nullptr;
}

Array<Name> PluginManager::GetDefaultEnabledPlugins() const
{
    Array<Name> pluginNames;

    for (const PluginDescriptor& descriptor : m_builtinPlugins)
    {
        if (descriptor.enabledByDefault)
        {
            pluginNames.PushBack(descriptor.name);
        }
    }

    return pluginNames;
}

bool PluginManager::IsPluginEnabled(Name pluginName) const
{
    return m_enabledPlugins.Contains(pluginName);
}

Array<Name> PluginManager::GetRuntimeModules(const Array<Name>& pluginNames) const
{
    Array<Name> orderedPlugins;
    Array<Name> visiting;

    for (Name pluginName : pluginNames)
    {
        ResolveEnabledPlugin(pluginName, orderedPlugins, visiting);
    }

    Array<Name> moduleNames;

    for (Name pluginName : orderedPlugins)
    {
        for (const PluginModuleRegistration& registration : GetRegisteredPluginModules())
        {
            if (CreateNameFromDynamicString(registration.pluginName) == pluginName && !String(registration.moduleName).EndsWith("Editor"))
            {
                moduleNames.PushBack(CreateNameFromDynamicString(registration.moduleName));
            }
        }
    }

    return moduleNames;
}

void PluginManager::ResolveEnabledPlugin(Name pluginName, Array<Name>& outOrdered, Array<Name>& visiting) const
{
    if (outOrdered.Contains(pluginName))
    {
        return;
    }

    const PluginDescriptor* descriptor = FindBuiltinPlugin(pluginName);

    if (descriptor == nullptr)
    {
        HYP_LOG(Plugins, Warning, "Plugin '{}' is enabled but isn't built in", pluginName);

        return;
    }

    if (visiting.Contains(pluginName))
    {
        HYP_LOG(Plugins, Error, "Plugin '{}' has a circular dependency", pluginName);

        return;
    }

    visiting.PushBack(pluginName);

    for (Name dependency : descriptor->dependencies)
    {
        ResolveEnabledPlugin(dependency, outOrdered, visiting);
    }

    visiting.PopBack();

    outOrdered.PushBack(pluginName);
}

void PluginManager::SetEnabledPlugins(const Array<Name>& pluginNames)
{
    Array<Name> orderedPlugins;
    Array<Name> visiting;

    for (Name pluginName : pluginNames)
    {
        ResolveEnabledPlugin(pluginName, orderedPlugins, visiting);
    }

    if (orderedPlugins == m_enabledPlugins)
    {
        return;
    }

    // unload in reverse, so a plugin goes before the ones it depends on
    for (size_t index = m_activeModules.Size(); index != 0; index--)
    {
        ActiveModule& activeModule = m_activeModules[index - 1];

        if (!orderedPlugins.Contains(activeModule.pluginName))
        {
            UnloadModule(activeModule);

            m_activeModules.EraseAt(index - 1);
        }
    }

    for (Name pluginName : orderedPlugins)
    {
        if (m_enabledPlugins.Contains(pluginName))
        {
            continue;
        }

        // a plugin's editor modules (named *Editor) load after its runtime ones
        Array<const PluginModuleRegistration*> registrations;

        for (const PluginModuleRegistration& registration : GetRegisteredPluginModules())
        {
            if (CreateNameFromDynamicString(registration.pluginName) == pluginName && !String(registration.moduleName).EndsWith("Editor"))
            {
                registrations.PushBack(&registration);
            }
        }

        for (const PluginModuleRegistration& registration : GetRegisteredPluginModules())
        {
            if (CreateNameFromDynamicString(registration.pluginName) == pluginName && String(registration.moduleName).EndsWith("Editor"))
            {
                registrations.PushBack(&registration);
            }
        }

        for (const PluginModuleRegistration* registrationPtr : registrations)
        {
            const PluginModuleRegistration& registration = *registrationPtr;

            ActiveModule activeModule;
            activeModule.pluginName = pluginName;
            activeModule.moduleName = CreateNameFromDynamicString(registration.moduleName);
            activeModule.module = registration.create();
            activeModule.context = MakeUnique<PluginContext>(pluginName, activeModule.moduleName, m_hostMode);

            activeModule.module->OnLoad(*activeModule.context);

            HYP_LOG(Plugins, Info, "Loaded plugin module {}.{}", pluginName, activeModule.moduleName);

            m_activeModules.PushBack(std::move(activeModule));
        }
    }

    m_enabledPlugins = std::move(orderedPlugins);
}

void PluginManager::UnloadModule(ActiveModule& activeModule)
{
    activeModule.module->OnUnload();
    activeModule.context->Release();

    HYP_LOG(Plugins, Info, "Unloaded plugin module {}.{}", activeModule.pluginName, activeModule.moduleName);

    activeModule.module.Reset();
    activeModule.context.Reset();
}

} // namespace Hyperion
