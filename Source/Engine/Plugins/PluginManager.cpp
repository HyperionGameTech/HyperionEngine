/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Engine/Plugins/PluginManager.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Core/Core.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Core/DLL/DynamicLibrary.hpp>

#include <Framework/CVarManager.hpp>

#include <Core/Reflection/ClassRegistry.hpp>

#include <Scene/ComponentInterface.hpp>

#include <Core/Utilities/StringUtil.hpp>

#ifdef HYP_DOTNET
#include <DotNET/DotNETHost.hpp>
#endif

#include <algorithm>

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(Plugins);

static CommandLineArgumentRegistration g_argPlugins {
    "plugins",
    {},
    "The plugins a game loads, comma separated",
    CommandLineArgumentFlags::NONE,
    CommandLineArgumentType::STRING
};

static CommandLineArgumentRegistration g_argPluginDir {
    "plugindir",
    {},
    "Directory for a development game to discover plugins when running as a standalone exe",
    CommandLineArgumentFlags::NONE,
    CommandLineArgumentType::STRING
};

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
    static size_t s_numInitialized = 0;

    Array<PluginModuleInitializer>& initializers = GetPluginModuleInitializers();

    for (; s_numInitialized < initializers.Size(); s_numInitialized++)
    {
        initializers[s_numInitialized]();
    }
}

namespace {

#if defined(HYP_WINDOWS)
static constexpr ANSIStringView PluginModuleLibraryPrefix = "hyperion-plugin-";
static constexpr ANSIStringView PluginModuleLibraryExtension = ".dll";
#elif defined(HYP_MACOS)
static constexpr ANSIStringView PluginModuleLibraryPrefix = "libhyperion-plugin-";
static constexpr ANSIStringView PluginModuleLibraryExtension = ".dylib";
#else
static constexpr ANSIStringView PluginModuleLibraryPrefix = "libhyperion-plugin-";
static constexpr ANSIStringView PluginModuleLibraryExtension = ".so";
#endif

static Array<FilePath> GetPluginLibraryDirectories()
{
    Array<FilePath> directories;

    if (const auto& pluginDirArgument = CoreApi::GetCommandLineArguments()["plugindir"]; pluginDirArgument.IsString())
    {
        directories.PushBack(FilePath(pluginDirArgument.ToString()));
    }

    const FilePath engineLibraryPath = FilePath(DynamicLibrary::GetPathOfAddress(reinterpret_cast<const void*>(&InitializePluginModules)).ToUtf8());

    if (!engineLibraryPath.Empty() && !directories.Contains(engineLibraryPath.BasePath() / "Plugins"))
    {
        directories.PushBack(engineLibraryPath.BasePath() / "Plugins");
    }

    const FilePath executableDirectory = CoreApi::GetExecutablePath() / "Plugins";

    if (!directories.Contains(executableDirectory))
    {
        directories.PushBack(executableDirectory);
    }

    return directories;
}

static String GetPluginModuleNameOfLibrary(const FilePath& library)
{
    const String fileName = library.Basename();

    if (!fileName.StartsWith(UTF8StringView(PluginModuleLibraryPrefix.Data())) || !fileName.EndsWith(UTF8StringView(PluginModuleLibraryExtension.Data())))
    {
        return String::empty;
    }

    return String(fileName.Substr(PluginModuleLibraryPrefix.Length(), fileName.Length() - PluginModuleLibraryExtension.Length()));
}

// runtime modules first, then editor modules (named *Editor), which use them
static Array<FilePath> FindPluginModuleLibraries(const FilePath& directory)
{
    Array<FilePath> runtimeLibraries;
    Array<FilePath> editorLibraries;

    if (!directory.IsDirectory())
    {
        return runtimeLibraries;
    }

    for (const FilePath& file : directory.GetAllFilesInDirectory())
    {
        const String moduleName = GetPluginModuleNameOfLibrary(file);

        if (moduleName.Empty())
        {
            continue;
        }

        if (moduleName.EndsWith("Editor"))
        {
            editorLibraries.PushBack(file);
        }
        else
        {
            runtimeLibraries.PushBack(file);
        }
    }

    std::sort(runtimeLibraries.Begin(), runtimeLibraries.End());
    std::sort(editorLibraries.Begin(), editorLibraries.End());

    runtimeLibraries.Concat(std::move(editorLibraries));

    return runtimeLibraries;
}

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

    DiscoverPlugins();

    if (m_hostMode == PluginHostMode::Game)
    {
        const CommandLineArguments& arguments = CoreApi::GetCommandLineArguments();

        if (arguments.Contains("plugins") && arguments["plugins"].IsString())
        {
            Array<Name> pluginNames;

            for (const String& pluginName : arguments["plugins"].ToString().Split(','))
            {
                if (pluginName.Any())
                {
                    pluginNames.PushBack(CreateNameFromDynamicString(pluginName.ToAnsi()));
                }
            }

            SetEnabledPlugins(pluginNames);
        }
        else
        {
            SetEnabledPlugins(GetDefaultEnabledPlugins());
        }
    }

    m_initialized = true;

    HYP_LOG(Plugins, Info, "Plugin manager initialized (host: {}, {} plugin(s) found)",
        m_hostMode == PluginHostMode::Editor ? "editor" : "game",
        m_plugins.Size());
}

void PluginManager::Shutdown()
{
    // preloaded libraries have CVars too, with or without an initialized manager
    // so we need to clear them anyway on shutdown.
    if (m_initialized)
    {
        SetEnabledPlugins({});
    }

    // the libraries stay loaded (the DynamicLibraryCache owns them), as task threads and static destructors can still run their code

    // but the cache closes them during static destruction, before the CVarManager is destroyed, so their
    // static CVars must be dropped now or ~CVarManager reads CVars in unmapped images
    for (const Name pluginName : m_pluginsWithLibrariesLoaded)
    {
        const PluginDescriptor* descriptor = FindPlugin(pluginName);

        if (descriptor == nullptr)
        {
            continue;
        }

        for (const FilePath& moduleLibrary : descriptor->moduleLibraries)
        {
            // skipped on load, so not loaded now
            if (m_hostMode != PluginHostMode::Editor && GetPluginModuleNameOfLibrary(moduleLibrary).EndsWith("Editor"))
            {
                continue;
            }

            DynamicLibrary* library =DynamicLibraryCache::GetInstance().LoadLibrary(PlatformString(String(moduleLibrary)));

            UIntPtr imageBegin = 0;
            UIntPtr imageEnd = 0;

            if (library != nullptr && library->GetImageRange(imageBegin, imageEnd))
            {
                CVarManager::GetInstance().UnregisterVarsInRange(imageBegin, imageEnd);
            }
        }
    }

    m_initialized = false;
}

void PluginManager::DiscoverPlugins()
{
    m_plugins.Clear();

    const FilePath& baseDirectory = CoreApi::GetBaseDirectory();
    const Array<FilePath> libraryDirectories = GetPluginLibraryDirectories();

    // the source tree's descriptors come first, as their folders hold the content; a packaged editor has them beside the libraries
    Array<FilePath> searchDirectories = {
        baseDirectory / "Source" / "Plugins",
        baseDirectory / "Tests" / "Plugins"
    };

    searchDirectories.Concat(libraryDirectories);

    // plugins are the ones whose modules are linked in (registered) or built as libraries
    Array<Name> pluginNames;

    for (const PluginModuleRegistration& registration : GetRegisteredPluginModules())
    {
        const Name pluginName = CreateNameFromDynamicString(registration.pluginName);

        if (!pluginNames.Contains(pluginName))
        {
            pluginNames.PushBack(pluginName);
        }
    }

#ifdef HYP_DYNAMIC_PLUGINS
    for (const FilePath& libraryDirectory : libraryDirectories)
    {
        if (!libraryDirectory.IsDirectory())
        {
            continue;
        }

        for (const FilePath& pluginDirectory : libraryDirectory.GetSubdirectories())
        {
            const Name pluginName = CreateNameFromDynamicString(pluginDirectory.Basename().ToAnsi());

            if (!pluginNames.Contains(pluginName) && FindPluginModuleLibraries(pluginDirectory).Any())
            {
                pluginNames.PushBack(pluginName);
            }
        }
    }
#endif

    for (Name pluginName : pluginNames)
    {
        PluginDescriptor descriptor;
        descriptor.enabledByDefault = true;

        for (const FilePath& searchDirectory : searchDirectories)
        {
            const FilePath descriptorPath = searchDirectory / pluginName.LookupString() / (String(pluginName.LookupString()) + ".hypplugin");

            if (!descriptorPath.Exists())
            {
                continue;
            }

            TResult<PluginDescriptor> descriptorResult = PluginDescriptor::Read(descriptorPath);

            if (descriptorResult.HasError())
            {
                HYP_LOG(Plugins, Warning, "{}", descriptorResult.GetError().GetMessage());

                break;
            }

            descriptor = descriptorResult.GetValue();

            break;
        }

        descriptor.name = pluginName;

        for (const PluginModuleRegistration& registration : GetRegisteredPluginModules())
        {
            if (StringHash(registration.pluginName) != pluginName)
            {
                continue;
            }

            const Name moduleName = CreateNameFromDynamicString(registration.moduleName);

            if (!descriptor.modules.Contains(moduleName))
            {
                descriptor.modules.PushBack(moduleName);
            }
        }

#ifdef HYP_DYNAMIC_PLUGINS
        for (const FilePath& libraryDirectory : libraryDirectories)
        {
            descriptor.moduleLibraries = FindPluginModuleLibraries(libraryDirectory / pluginName.LookupString());

            if (descriptor.moduleLibraries.Any())
            {
                break;
            }
        }

        for (const FilePath& moduleLibrary : descriptor.moduleLibraries)
        {
            const Name moduleName = CreateNameFromDynamicString(GetPluginModuleNameOfLibrary(moduleLibrary).ToAnsi());

            if (!descriptor.modules.Contains(moduleName))
            {
                descriptor.modules.PushBack(moduleName);
            }
        }
#endif

        HYP_LOG(Plugins, Info, "Found plugin '{}' v{}", pluginName, descriptor.version);

        m_plugins.PushBack(std::move(descriptor));
    }
}

const PluginDescriptor* PluginManager::FindPlugin(StringHash pluginName) const
{
    auto it = m_plugins.FindIf([pluginName](const PluginDescriptor& descriptor)
        {
            return descriptor.name == pluginName;
        });

    return it != m_plugins.End() ? &*it : nullptr;
}

Array<Name> PluginManager::GetDefaultEnabledPlugins() const
{
    Array<Name> pluginNames;

    for (const PluginDescriptor& descriptor : m_plugins)
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

Array<Name> PluginManager::GetRuntimeModules(Span<const Name> pluginNames) const
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
        const PluginDescriptor* descriptor = FindPlugin(pluginName);

        if (!descriptor)
        {
            continue;
        }

        for (Name moduleName : descriptor->modules)
        {
            if (!ANSIStringView(moduleName.LookupString()).EndsWith("Editor"))
            {
                moduleNames.PushBack(moduleName);
            }
        }
    }

    return moduleNames;
}

bool PluginManager::ArePluginLibrariesLoaded(Name pluginName) const
{
#ifdef HYP_DYNAMIC_PLUGINS
    return m_pluginsWithLibrariesLoaded.Contains(pluginName);
#else
    return FindPlugin(pluginName) != nullptr;
#endif
}

void PluginManager::LoadAllPluginLibraries()
{
    if (m_plugins.Empty())
    {
        DiscoverPlugins();
    }

    for (const PluginDescriptor& descriptor : m_plugins)
    {
        LoadPluginLibraries(descriptor);
    }
}

void PluginManager::PreloadPlugins(Span<const Name> pluginNames)
{
    // a process that never initializes the manager (the cook) still needs the plugins' classes
    if (m_plugins.Empty())
    {
        DiscoverPlugins();
    }

    Array<Name> orderedPlugins;
    Array<Name> visiting;

    for (Name pluginName : pluginNames)
    {
        ResolveEnabledPlugin(pluginName, orderedPlugins, visiting);
    }

    for (Name pluginName : orderedPlugins)
    {
        if (const PluginDescriptor* descriptor = FindPlugin(pluginName))
        {
            LoadPluginCode(*descriptor);
        }
    }
}

void PluginManager::LoadPluginCode(const PluginDescriptor& descriptor)
{
    LoadPluginLibraries(descriptor);

#ifdef HYP_DOTNET
    for (Name moduleName : descriptor.modules)
    {
        const ANSIStringView moduleNameString = moduleName.LookupString();

        if ((m_hostMode != PluginHostMode::Editor && moduleNameString.EndsWith("Editor"))
            || m_modulesAnnouncedToManaged.Contains(moduleName))
        {
            continue;
        }

        m_modulesAnnouncedToManaged.PushBack(moduleName);

        DotNETHost::GetInstance().NotifyPluginModuleLoaded(moduleNameString);
    }
#endif
}

void PluginManager::LoadPluginLibraries(const PluginDescriptor& descriptor)
{
    if (descriptor.moduleLibraries.Empty() || m_pluginsWithLibrariesLoaded.Contains(descriptor.name))
    {
        return;
    }

    if (descriptor.engineMinVersion.Any())
    {
        uint32 minMajor = 0;
        uint32 minMinor = 0;
        uint32 minPatch = 0;

        if (!ParseVersionString(descriptor.engineMinVersion, minMajor, minMinor, minPatch)
            || HYP_VERSION_MAJOR < minMajor
            || (HYP_VERSION_MAJOR == minMajor && HYP_VERSION_MINOR < minMinor)
            || (HYP_VERSION_MAJOR == minMajor && HYP_VERSION_MINOR == minMinor && HYP_VERSION_PATCH < minPatch))
        {
            HYP_LOG(Plugins, Warning, "Plugin '{}' requires engine version >= {} but this is {}.{}.{}",
                descriptor.name,
                descriptor.engineMinVersion,
                HYP_VERSION_MAJOR,
                HYP_VERSION_MINOR,
                HYP_VERSION_PATCH);

            return;
        }
    }

    m_pluginsWithLibrariesLoaded.PushBack(descriptor.name);

    // a module library can link a dependency's, which the loader only finds once that one is loaded
    for (Name dependency : descriptor.dependencies)
    {
        if (const PluginDescriptor* dependencyDescriptor = FindPlugin(dependency))
        {
            LoadPluginLibraries(*dependencyDescriptor);
        }
    }

    // the plugin's classes register as its libraries load, in no particular order, so they're initialized once all of them are in
    ClassRegistry::GetInstance().BeginDeferredInitialization();

    for (const FilePath& moduleLibrary : descriptor.moduleLibraries)
    {
        // an editor module only runs in the editor, and needs the editor's code
        if (m_hostMode != PluginHostMode::Editor && GetPluginModuleNameOfLibrary(moduleLibrary).EndsWith("Editor"))
        {
            continue;
        }

        if (DynamicLibraryCache::GetInstance().LoadLibrary(PlatformString(String(moduleLibrary))) == nullptr)
        {
            HYP_LOG(Plugins, Error, "Failed to load plugin '{}' library at {}", descriptor.name, moduleLibrary);

            continue;
        }

        HYP_LOG(Plugins, Info, "Loaded plugin '{}' library {}", descriptor.name, moduleLibrary.Basename());
    }

    // the libraries' static initializers added their modules' initializers, which register their classes and modules
    InitializePluginModules();

    ClassRegistry::GetInstance().EndDeferredInitialization();

    // like a game module loaded after startup, its components join the registry now
    ComponentInterfaceRegistry::GetInstance().PublishModuleComponents(/* resident */ true);
}

void PluginManager::ResolveEnabledPlugin(
    StringHash pluginName,
    Array<Name>& outOrdered,
    Array<Name>& visiting) const
{
    if (outOrdered.Contains(pluginName))
    {
        return;
    }

    const PluginDescriptor* descriptor = FindPlugin(pluginName);

    if (descriptor == nullptr)
    {
        HYP_LOG(Plugins, Warning, "Plugin '{}' is enabled but wasn't found", Name(pluginName));

        return;
    }

    if (visiting.Contains(pluginName))
    {
        HYP_LOG(Plugins, Error, "Plugin '{}' has a circular dependency", Name(pluginName));

        return;
    }

    visiting.PushBack(Name(pluginName));

    for (Name dependency : descriptor->dependencies)
    {
        ResolveEnabledPlugin(dependency, outOrdered, visiting);
    }

    visiting.PopBack();

    outOrdered.PushBack(Name(pluginName));
}

void PluginManager::SetEnabledPlugins(Span<const Name> pluginNames)
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

    for (Name pluginName : m_enabledPlugins)
    {
        if (!orderedPlugins.Contains(pluginName))
        {
            SetPluginContentMounted(pluginName, false);
        }
    }

    for (Name pluginName : orderedPlugins)
    {
        if (m_enabledPlugins.Contains(pluginName))
        {
            continue;
        }

        if (const PluginDescriptor* descriptor = FindPlugin(pluginName))
        {
            LoadPluginCode(*descriptor);
        }

        // before its modules load, so they can use its assets
        SetPluginContentMounted(pluginName, true);

        // a plugin's editor modules (named *Editor) load after its runtime ones
        Array<const PluginModuleRegistration*> registrations;

        for (const PluginModuleRegistration& registration : GetRegisteredPluginModules())
        {
            if (StringHash(registration.pluginName) == pluginName
                && !registration.moduleName.EndsWith("Editor"))
            {
                registrations.PushBack(&registration);
            }
        }

        for (const PluginModuleRegistration& registration : GetRegisteredPluginModules())
        {
            if (StringHash(registration.pluginName) == pluginName
                && registration.moduleName.EndsWith("Editor"))
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

Array<FilePath> PluginManager::GetPluginContentDirectories(Span<const StringHash> pluginNames)
{
    if (m_plugins.Empty())
    {
        DiscoverPlugins();
    }

    Array<Name> orderedPlugins;
    Array<Name> visiting;

    for (StringHash pluginName : pluginNames)
    {
        ResolveEnabledPlugin(pluginName, orderedPlugins, visiting);
    }

    Array<FilePath> contentDirectories;

    for (Name pluginName : orderedPlugins)
    {
        if (const PluginDescriptor* descriptor = FindPlugin(pluginName);
                descriptor && (descriptor->directory / "Content").IsDirectory())
        {
            contentDirectories.PushBack(descriptor->directory / "Content");
        }
    }

    return contentDirectories;
}

void PluginManager::SetPluginContentMounted(Name pluginName, bool mounted) const
{
    const PluginDescriptor* descriptor = FindPlugin(pluginName);

    if (!descriptor)
    {
        return;
    }

    const FilePath contentDirectory = descriptor->directory / "Content";

    if (!contentDirectory.IsDirectory())
    {
        return;
    }

    const Handle<AssetRegistry> engineRegistry = GetEngineAssetRegistry();

    // at shutdown the registry can be gone already, which is fine for an unmount
    if (!engineRegistry.IsValid())
    {
        if (mounted)
        {
            HYP_LOG(Plugins, Warning, "No engine asset registry to add the content of plugin {} to", pluginName);
        }

        return;
    }

    if (mounted)
    {
        engineRegistry->AddContentRoot(contentDirectory);
    }
    else
    {
        engineRegistry->RemoveContentRoot(contentDirectory);
    }
}

} // namespace Hyperion
