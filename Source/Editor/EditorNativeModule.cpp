/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorNativeModule.hpp>
#include <Editor/EditorNativeReload.hpp>
#include <Editor/EditorGameModuleCommon.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorProjectLayout.hpp>
#include <Editor/EditorSubsystem.hpp>

#include <Framework/Game.hpp>
#include <Framework/CVarManager.hpp>

#include <Scene/Scene.hpp>
#include <Scene/DetachedScene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/ComponentInterface.hpp>
#include <Scene/Systems/ScriptSystem.hpp>

#include <Core/DLL/DynamicLibrary.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/TaskSystem.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/ObjectPool.hpp>
#include <Core/Reflection/TypeInfo.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#ifdef HYP_DOTNET
#include <DotNET/DotNETHost.hpp>
#include <DotNET/Assembly.hpp>
#include <DotNET/ManagedClass.hpp>
#endif

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

enum class NativeModuleFlags : uint32
{
    None = 0x0,
    Loaded = 0x1,
    WarnedRebuilt = 0x2,
    HasPlayed = 0x4,
    HasBindings = 0x8
};

HYP_MAKE_ENUM_FLAGS(NativeModuleFlags);

enum class RetiredModuleFlags : uint32
{
    None = 0x0,
    HasPlayed = 0x1,
    Warned = 0x2,
    WarnedObjectsMade = 0x4
};

HYP_MAKE_ENUM_FLAGS(RetiredModuleFlags);

namespace {

using GetGameClassNameFn = const char* (*)();
using GetEngineVersionFn = void (*)(uint32*, uint32*, uint32*);

constexpr int64 ManagedCollectIntervalMs = 2000;
constexpr int64 StaysLoadedWarningDelayMs = 30000;

// the copy of a game module build that's loaded
struct NativeModuleImage
{
    FilePath copyPath;
    UIntPtr imageBegin = 0;
    UIntPtr imageEnd = 0;

    // without an image range, what the module registered can't be told apart, so it stays loaded
    bool CanUnload() const
    {
        return imageEnd > imageBegin;
    }
};

struct LoadedNativeModule : GameModuleBase
{
    NativeModuleImage image;
    String gameClassName;
    EnumFlags<NativeModuleFlags> flags = NativeModuleFlags::None;
};

// a build that was replaced; unlike a loaded module it's never polled or reloaded, only freed once nothing uses it
struct RetiredNativeModule
{
    NativeModuleImage image;
    Array<const Class*> classes;
    Array<ObjectContainerBase*> containers;
    Time retiredTime;
    Time lastCollectTime;
    Time lastObjectScanTime;
    uint32 requiredCollection = 0;
    uint32 playsStoppedSinceRetired = 0;
    EnumFlags<RetiredModuleFlags> flags = RetiredModuleFlags::None;
};

// full C# collections with their finalizers, run off the sim thread since finalizers can need it
class ManagedGarbageWaiter
{
public:
    static bool CanWait()
    {
#ifdef HYP_DOTNET
        return DotNETHost::GetInstance().GetGlobalFunctions().collectGarbageAndWaitFunction != nullptr;
#else
        return false;
#endif
    }

    // the collection count to wait for: one that starts after now, so a collection already running doesn't count
    uint32 RequestCollectionAfterNow() const
    {
        return m_completedCollections.Get(MemoryOrder::ACQUIRE) + (m_isCollecting.Get(MemoryOrder::ACQUIRE) ? 2 : 1);
    }

    bool HasCompleted(uint32 collection) const
    {
        return m_completedCollections.Get(MemoryOrder::ACQUIRE) >= collection;
    }

    void StartCollecting()
    {
#ifdef HYP_DOTNET
        TriggerGCFunction collectAndWait = DotNETHost::GetInstance().GetGlobalFunctions().collectGarbageAndWaitFunction;

        if (collectAndWait == nullptr || m_isCollecting.Exchange(true, MemoryOrder::ACQUIRE_RELEASE))
        {
            return;
        }

        TaskSystem::GetInstance().Enqueue(
            [this, collectAndWait]()
            {
                collectAndWait();

                m_completedCollections.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
                m_isCollecting.Set(false, MemoryOrder::RELEASE);
            },
            TaskThreadPoolName::THREAD_POOL_BACKGROUND,
            TaskEnqueueFlags::FIRE_AND_FORGET);
#endif
    }

private:
    AtomicVar<uint32> m_completedCollections { 0 };
    AtomicVar<bool> m_isCollecting { false };
};

// guarded by GetGameModulesMutex(), except the garbage waiter, which its background task also updates
struct NativeModuleState
{
    Array<UniquePtr<LoadedNativeModule>> loadedModules;
    Array<RetiredNativeModule> retiredModules;

    // classes of retired builds whose scene nodes wait to be rebuilt on the latest build; kept across failed builds
    Array<const Class*> classesToRebuild;

    bool wasSimulating = false;

    ManagedGarbageWaiter garbageWaiter;
};

NativeModuleState& GetNativeModuleState()
{
    static NativeModuleState s_state;

    return s_state;
}

void EvictModuleTypeInfos(const RetiredNativeModule& retiredModule)
{
    Array<TypeId> typeIds;

    for (const Class* cls : retiredModule.classes)
    {
        typeIds.PushBack(cls->GetTypeId());
    }

    TypeInfo_EvictForLibrary(retiredModule.image.imageBegin, retiredModule.image.imageEnd, typeIds);
}

RetiredNativeModule RetireModule(const LoadedNativeModule& loadedModule)
{
    RetiredNativeModule retiredModule;
    retiredModule.image = loadedModule.image;

    retiredModule.classes = ClassRegistry::GetInstance().UnregisterClassesInRange(
        loadedModule.image.imageBegin,
        loadedModule.image.imageEnd);

    retiredModule.containers = GetObjectContainerMap().Detach(retiredModule.classes);

    retiredModule.retiredTime = Time::Now();

    if (loadedModule.flags & NativeModuleFlags::HasPlayed)
    {
        retiredModule.flags |= RetiredModuleFlags::HasPlayed;
    }

    EvictModuleTypeInfos(retiredModule);

    CVarManager::GetInstance().UnregisterVarsInRange(
        loadedModule.image.imageBegin,
        loadedModule.image.imageEnd);

    ComponentInterfaceRegistry::GetInstance().UnregisterComponentsInRange(
        loadedModule.image.imageBegin,
        loadedModule.image.imageEnd);

    return retiredModule;
}

void UnloadRetiredModule(RetiredNativeModule& retiredModule)
{
    // its code may have made more TypeInfos since it was retired
    EvictModuleTypeInfos(retiredModule);

    // the containers' destructors are in the module
    for (ObjectContainerBase* container : retiredModule.containers)
    {
        delete container;
    }

    retiredModule.containers.Clear();

#ifdef HYP_DOTNET
    // the game's C# bindings assembly stays loaded, and unloading it at shutdown would reach its classes in this module
    for (const Class* cls : retiredModule.classes)
    {
        if (SharedPtr<dotnet::ManagedClass> managedClass = cls->GetManagedClass();
                managedClass && managedClass->GetClass() == cls)
        {
            managedClass->DetachClass();
        }

        cls->SetManagedClass(nullptr);
    }
#endif

    DynamicLibraryCache::GetInstance().UnloadLibrary(PlatformString(String(retiredModule.image.copyPath)));

    if (retiredModule.image.copyPath.Exists() && !retiredModule.image.copyPath.Remove())
    {
        HYP_LOG(Editor, Warning, "Unloaded previous game module build {}, but its copy couldn't be deleted", retiredModule.image.copyPath);

        return;
    }

    HYP_LOG(Editor, Info, "Unloaded previous game module build {}", retiredModule.image.copyPath);
}

bool IsRetiredModuleClass(const Class* cls)
{
    const NativeModuleState& state = GetNativeModuleState();

    for (const RetiredNativeModule& retiredModule : state.retiredModules)
    {
        if (retiredModule.classes.Contains(cls))
        {
            return true;
        }
    }

    return false;
}

// a module class is held by its instances and ClassRefs (component containers and boxed values), as a dynamic class is
Array<const Class*> GetClassesInUse(const RetiredNativeModule& retiredModule)
{
    Array<const Class*> classesInUse;

    for (const Class* cls : retiredModule.classes)
    {
        if (cls->GetModuleRefCount() != 0)
        {
            classesInUse.PushBack(cls);
        }
    }

    return classesInUse;
}

const Class* FindObjectForModule(const NativeModuleImage& image)
{
    const Class* found = nullptr;

    auto checkHeader = [&found, &image](const ObjectHeader* header)
    {
        if (found || header->GetRefCountStrong() <= 0)
        {
            return;
        }

        const UIntPtr vtable = *reinterpret_cast<const UIntPtr*>(ObjectHeader::GetObjectPointer(const_cast<ObjectHeader*>(header)));

        if (vtable >= image.imageBegin && vtable < image.imageEnd)
        {
            found = header->cls;
        }
    };

    auto checkContainer = [&found, &checkHeader](TypeId, ObjectContainerBase* container)
    {
        if (!found && container != nullptr)
        {
            container->ForEachHeader(checkHeader);
        }
    };

    GetObjectContainerMap().ForEachContainer(checkContainer);

    return found;
}

void UpdateRetiredModules()
{
    NativeModuleState& state = GetNativeModuleState();

    const Time now = Time::Now();

    for (size_t index = state.retiredModules.Size(); index-- > 0;)
    {
        RetiredNativeModule& retiredModule = state.retiredModules[index];

        const Array<const Class*> classesInUse = GetClassesInUse(retiredModule);

        if (classesInUse.Any())
        {
            retiredModule.requiredCollection = 0;

            if (int64(now - retiredModule.lastCollectTime) >= ManagedCollectIntervalMs)
            {
                CollectManagedGarbage();

                retiredModule.lastCollectTime = now;
            }

            if (!(retiredModule.flags & RetiredModuleFlags::Warned) && int64(now - retiredModule.retiredTime) >= StaysLoadedWarningDelayMs)
            {
                String classNames;

                for (const Class* cls : classesInUse)
                {
                    if (classNames.Any())
                    {
                        classNames += ", ";
                    }

                    classNames += *cls->GetName();
                }

                HYP_LOG(Editor, Warning, "Previous game module build {} stays loaded while objects of {} are alive", retiredModule.image.copyPath, classNames);

                retiredModule.flags |= RetiredModuleFlags::Warned;
            }

            continue;
        }

        // what its code made during play can outlive the session, held by the editor until the next play
        if ((retiredModule.flags & RetiredModuleFlags::HasPlayed) && retiredModule.playsStoppedSinceRetired == 0)
        {
            continue;
        }

        // C# objects that are garbage but not finalized yet can still hold what its code made
        if (ManagedGarbageWaiter::CanWait())
        {
            if (retiredModule.requiredCollection == 0)
            {
                retiredModule.requiredCollection = state.garbageWaiter.RequestCollectionAfterNow();
            }

            if (!state.garbageWaiter.HasCompleted(retiredModule.requiredCollection))
            {
                state.garbageWaiter.StartCollecting();

                continue;
            }

            retiredModule.requiredCollection = 0;

            if (GetClassesInUse(retiredModule).Any())
            {
                continue;
            }
        }

        if (int64(now - retiredModule.lastObjectScanTime) < ManagedCollectIntervalMs)
        {
            continue;
        }

        retiredModule.lastObjectScanTime = now;

        if (const Class* objectClass = FindObjectForModule(retiredModule.image))
        {
            if (!(retiredModule.flags & RetiredModuleFlags::WarnedObjectsMade) && int64(now - retiredModule.retiredTime) >= StaysLoadedWarningDelayMs)
            {
                HYP_LOG(Editor, Warning, "Previous game module build {} stays loaded while objects its code made are alive (a {} among them)", retiredModule.image.copyPath, objectClass->GetName());

                retiredModule.flags |= RetiredModuleFlags::WarnedObjectsMade;
            }

            continue;
        }

        UnloadRetiredModule(retiredModule);

        state.retiredModules.EraseAt(index);
    }
}

String GetModuleTargetName(const FilePath& modulePath)
{
    String moduleStem = FilePath(modulePath.Basename()).StripExtension();

#ifndef HYP_WINDOWS
    if (moduleStem.StartsWith("lib"))
    {
        moduleStem = String(moduleStem.Substr(3));
    }
#endif

    return moduleStem;
}

FilePath GetModuleBindingsPath(const FilePath& modulePath)
{
    return modulePath.BasePath() / (GetModuleTargetName(modulePath) + ".Bindings.dll");
}

String CheckModuleEngineVersion(const FilePath& modulePath)
{
    const FilePath stampPath = modulePath.BasePath() / (GetModuleTargetName(modulePath) + ".EngineVersion.txt");

    if (!stampPath.Exists())
    {
        return HYP_FORMAT("Game module {} has no engine version stamp ({}), so it was built for an older engine.", modulePath, stampPath);
    }

    const ByteBuffer stampBytes = FileByteReader(stampPath).Read();
    const String builtVersion = String(ConstByteView(stampBytes.Data(), stampBytes.Size())).Trimmed();
    const String engineVersion = HYP_FORMAT("{}.{}.{}", HYP_VERSION_MAJOR, HYP_VERSION_MINOR, HYP_VERSION_PATCH);

    if (builtVersion != engineVersion)
    {
        return HYP_FORMAT("Game module {} was built for engine version {}, currently running on version {}.", modulePath, builtVersion, engineVersion);
    }

    return String();
}

#ifdef HYP_DOTNET
bool LoadModuleBindings(const FilePath& modulePath)
{
    const FilePath bindingsPath = GetModuleBindingsPath(modulePath);

    if (!bindingsPath.Exists() || !DotNETHost::GetInstance().IsInitialized())
    {
        return false;
    }

    TResult<FilePath> copyResult = CopyModule(bindingsPath, "NativeModuleBindings");

    if (copyResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to copy game bindings {}: {}", bindingsPath, copyResult.GetError().GetMessage());

        return false;
    }

    if (!DotNETHost::GetInstance().LoadAssembly(copyResult.GetValue().Data()))
    {
        HYP_LOG(Editor, Error, "Failed to load game bindings {} of {}. Rebuild the game.", bindingsPath, modulePath);

        return false;
    }

    return true;
}
#endif

void LoadModuleCopy(LoadedNativeModule& loadedModule)
{
    if (const String versionError = CheckModuleEngineVersion(loadedModule.modulePath); versionError.Any())
    {
        loadedModule.error = versionError + " Rebuild the game.";

        return;
    }

    DynamicLibrary* library = DynamicLibraryCache::GetInstance().LoadLibrary(PlatformString(String(loadedModule.image.copyPath)));

    if (library == nullptr)
    {
        loadedModule.error = HYP_FORMAT("Failed to load game module {}", loadedModule.modulePath);

        return;
    }

    loadedModule.flags |= NativeModuleFlags::Loaded;

    library->GetImageRange(loadedModule.image.imageBegin, loadedModule.image.imageEnd);

    const GetGameClassNameFn getGameClassName = reinterpret_cast<GetGameClassNameFn>(library->GetFunction("HypGameModule_GetGameClassName"));
    const GetEngineVersionFn getEngineVersion = reinterpret_cast<GetEngineVersionFn>(library->GetFunction("HypGameModule_GetEngineVersion"));

    uint32 versionMajor = 0;
    uint32 versionMinor = 0;
    uint32 versionPatch = 0;

    if (getGameClassName == nullptr || getEngineVersion == nullptr)
    {
        loadedModule.error = HYP_FORMAT("Game module {} doesn't export HypGameModule_GetGameClassName / HypGameModule_GetEngineVersion", loadedModule.modulePath);
    }
    else if (getEngineVersion(&versionMajor, &versionMinor, &versionPatch);
        versionMajor != HYP_VERSION_MAJOR || versionMinor != HYP_VERSION_MINOR)
    {
        loadedModule.error = HYP_FORMAT("Game module {} was built for engine {}.{}.{} (this is {}.{}.{}). Rebuild the game.",
            loadedModule.modulePath, versionMajor, versionMinor, versionPatch, HYP_VERSION_MAJOR, HYP_VERSION_MINOR, HYP_VERSION_PATCH);
    }
    else
    {
        loadedModule.gameClassName = getGameClassName();

        const Class* gameClass = GetClass(StringHash(loadedModule.gameClassName.Data()));

        if (gameClass == nullptr || !gameClass->IsDerivedFrom(Game::StaticClass()))
        {
            loadedModule.error = HYP_FORMAT("Game module {} names Game class '{}', which isn't a registered Game subclass", loadedModule.modulePath, loadedModule.gameClassName);
        }
        else
        {
#ifdef HYP_DOTNET
            if (LoadModuleBindings(loadedModule.modulePath))
            {
                loadedModule.flags |= NativeModuleFlags::HasBindings;
            }

            // The class has no C# counterpart, so the managed editor sees instances of it as plain Game objects
            if (gameClass->GetManagedClass() == nullptr)
            {
                gameClass->SetManagedClass(Game::StaticClass()->GetManagedClass());
            }
#endif
        }
    }

    if (loadedModule.error.Empty())
    {
        ComponentInterfaceRegistry::GetInstance().PublishModuleComponents();

        return;
    }

    // nothing was made from it yet, so a failed build can go right away and be retried once rebuilt
    if (loadedModule.image.CanUnload())
    {
        RetiredNativeModule failedModule = RetireModule(loadedModule);
        UnloadRetiredModule(failedModule);

        loadedModule.flags &= ~NativeModuleFlags::Loaded;
    }
}

/// Loading the new build before retiring the current one isn't possible, since both register the same class names. So a build that
/// fails to load leaves no module active
TResult<LoadedNativeModule*> LoadLatestModule(const FilePath& modulePath, const FilePath& projectDirectory)
{
    NativeModuleState& state = GetNativeModuleState();

    const Time timestamp = modulePath.LastModifiedTimestamp();

    LoadedNativeModule* currentModule = FindGameModule(state.loadedModules, modulePath);

    if (currentModule != nullptr)
    {
        if (timestamp <= currentModule->loadedTimestamp)
        {
            if (currentModule->flags & NativeModuleFlags::HasBindings)
            {
                ScriptSystem::SetGameAssembly(projectDirectory, GetModuleBindingsPath(modulePath), /* isNewBuild */ false);
            }

            return currentModule;
        }

        if ((currentModule->flags & NativeModuleFlags::Loaded) && !currentModule->image.CanUnload())
        {
            if (!(currentModule->flags & NativeModuleFlags::WarnedRebuilt))
            {
                HYP_LOG(Editor, Warning, "Game module {} was rebuilt since it was loaded, restart the editor to use the new build", modulePath);

                currentModule->flags |= NativeModuleFlags::WarnedRebuilt;
            }

            return currentModule;
        }
    }

    TResult<FilePath> copyResult = CopyModule(modulePath, "NativeModules");

    if (copyResult.HasError())
    {
        // most likely still being written by the linker
        if (currentModule != nullptr && (currentModule->flags & NativeModuleFlags::Loaded) && currentModule->error.Empty())
        {
            HYP_LOG(Editor, Warning, "Using the previous build of game module {}: {}", modulePath, copyResult.GetError().GetMessage());

            return currentModule;
        }

        return copyResult.GetError();
    }

    const bool isReload = currentModule != nullptr && (currentModule->flags & NativeModuleFlags::Loaded);

    if (isReload)
    {
        const RetiredNativeModule& retiredModule = state.retiredModules.PushBack(RetireModule(*currentModule));

        state.classesToRebuild.Concat(retiredModule.classes);
    }

    if (currentModule == nullptr)
    {
        currentModule = state.loadedModules.PushBack(MakeUnique<LoadedNativeModule>()).Get();
    }

    // reloaded in place, so pointers to it stay valid
    LoadedNativeModule& loadedModule = *currentModule;
    loadedModule = LoadedNativeModule {};
    loadedModule.modulePath = modulePath;
    loadedModule.image.copyPath = copyResult.GetValue();
    loadedModule.loadedTimestamp = timestamp;

    LoadModuleCopy(loadedModule);

    if (loadedModule.error.Any())
    {
        HYP_LOG(Editor, Error, "{}", loadedModule.error);

        if (state.classesToRebuild.Any())
        {
            HYP_LOG(Editor, Warning, "No build of game module {} is active; existing objects of its classes keep running the previous build until a build loads", modulePath);
        }
    }
    else
    {
        HYP_LOG(Editor, Info, "{} game module {} (Game class: {})", isReload ? "Reloaded" : "Loaded", modulePath, loadedModule.gameClassName);
    }

    if (loadedModule.flags & NativeModuleFlags::HasBindings)
    {
        ScriptSystem::SetGameAssembly(projectDirectory, GetModuleBindingsPath(modulePath), /* isNewBuild */ true);
    }

    return &loadedModule;
}

// the Game class to replace the project's Game with, if the project's Game is of a retired build
String GetRetiredProjectGameReplacement(EditorProject* project, const LoadedNativeModule& loadedModule)
{
    const Handle<Game>& oldGame = project->GetGame();

    if (!oldGame.IsValid() || oldGame->GetWorld().IsValid() || loadedModule.error.Any() || !IsRetiredModuleClass(oldGame->InstanceClass()))
    {
        return String();
    }

    return loadedModule.gameClassName;
}

void ReplaceProjectGameWithClass(EditorProject* project, const String& gameClassName)
{
    Handle<Game> newGame = Game::CreateGame(StringHash(gameClassName.Data()));

    if (!newGame.IsValid())
    {
        HYP_LOG(Editor, Error, "Failed to create an instance of Game class '{}'", gameClassName);

        return;
    }

    ReplaceProjectGame(project, project->GetGame(), std::move(newGame));
}

Result ReloadNativeGameModule(EditorProject* project)
{
    NativeModuleState& state = GetNativeModuleState();

    const FilePath modulePath = GetNativeModulePath(project->GetFilePath());

    if (!modulePath.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Cannot reload the game module: {} isn't built", modulePath);
    }

    String gameClassToCreate;

    {
        Mutex::Guard guard(GetGameModulesMutex());

        if (const LoadedNativeModule* currentModule = FindGameModule(state.loadedModules, modulePath);
            currentModule != nullptr && (currentModule->flags & NativeModuleFlags::Loaded) && modulePath.LastModifiedTimestamp() <= currentModule->loadedTimestamp)
        {
            HYP_LOG(Editor, Info, "Game module {} is up to date", modulePath);

            return {};
        }

        TResult<LoadedNativeModule*> moduleResult = LoadLatestModule(modulePath, project->GetFilePath().BasePath());

        if (moduleResult.HasError())
        {
            return moduleResult.GetError();
        }

        if (moduleResult.GetValue()->error.Any())
        {
            return HYP_MAKE_ERROR(Error, "{}", moduleResult.GetValue()->error);
        }

        gameClassToCreate = GetRetiredProjectGameReplacement(project, *moduleResult.GetValue());
    }

    // scene nodes of the retired build are rebuilt by the next UpdateGameModules
    if (gameClassToCreate.Any())
    {
        ReplaceProjectGameWithClass(project, gameClassToCreate);
    }

    return {};
}

} // namespace

void RetireNativeModule(const FilePath& modulePath)
{
    NativeModuleState& state = GetNativeModuleState();

    Mutex::Guard guard(GetGameModulesMutex());

    for (size_t index = 0; index < state.loadedModules.Size(); index++)
    {
        LoadedNativeModule& loadedModule = *state.loadedModules[index];

        if (loadedModule.modulePath != modulePath)
        {
            continue;
        }

        if ((loadedModule.flags & NativeModuleFlags::Loaded) && loadedModule.image.CanUnload())
        {
            state.retiredModules.PushBack(RetireModule(loadedModule));
        }

        state.loadedModules.EraseAt(index);

        return;
    }
}

EDITOR_API String GetNativeProjectName(const FilePath& projectFilepath)
{
    return ReadProjectModuleName(projectFilepath);
}

EDITOR_API FilePath GetNativeModulePath(const FilePath& projectFilepath)
{
#ifdef HYP_WINDOWS
    const String fileName = GetNativeProjectName(projectFilepath) + "Game.dll";
#elif defined(HYP_MACOS)
    const String fileName = String("lib") + GetNativeProjectName(projectFilepath) + "Game.dylib";
#else
    const String fileName = String("lib") + GetNativeProjectName(projectFilepath) + "Game.so";
#endif

    return projectFilepath.BasePath() / "Binaries" / String(GetModulePlatformDirectory()) / fileName;
}

EDITOR_API FilePath GetNativeBuildScriptPath(const FilePath& projectFilepath)
{
#ifdef HYP_WINDOWS
    static constexpr const char* BuildScriptName = "Build.bat";
#else
    static constexpr const char* BuildScriptName = "Build.sh";
#endif

    return projectFilepath.BasePath() / "Build" / String(GetModulePlatformDirectory()) / BuildScriptName;
}

EDITOR_API String CheckNativeModuleEngineVersion(const FilePath& projectFilepath)
{
    const FilePath modulePath = GetNativeModulePath(projectFilepath);

    return modulePath.Exists() ? CheckModuleEngineVersion(modulePath) : String();
}

EDITOR_API TResult<Handle<Game>> CreateGameFromNativeModule(const FilePath& projectFilepath)
{
    const FilePath modulePath = GetNativeModulePath(projectFilepath);

    if (!modulePath.Exists())
    {
        return Handle<Game>();
    }

    String gameClassName;

    {
        Mutex::Guard guard(GetGameModulesMutex());

        TResult<LoadedNativeModule*> moduleResult = LoadLatestModule(modulePath, projectFilepath.BasePath());

        if (moduleResult.HasError())
        {
            return moduleResult.GetError();
        }

        if (moduleResult.GetValue()->error.Any())
        {
            return HYP_MAKE_ERROR(Error, "{}", moduleResult.GetValue()->error);
        }

        gameClassName = moduleResult.GetValue()->gameClassName;
    }

    Handle<Game> game = Game::CreateGame(StringHash(gameClassName.Data()));

    if (!game.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create an instance of Game class '{}' from {}", gameClassName, modulePath);
    }

    return game;
}

EDITOR_API Result ReloadGameModule(EditorSubsystem* subsystem)
{
    if (subsystem->GetCurrentProject().IsValid() && subsystem->IsSimulating())
    {
        return HYP_MAKE_ERROR(Error, "Cannot reload the game module while simulating, it's reloaded when play starts");
    }

    return ReloadGameModule(subsystem->GetCurrentProject().Get());
}

EDITOR_API Result ReloadGameModule(EditorProject* project)
{
    if (project == nullptr || !project->IsSaved())
    {
        return HYP_MAKE_ERROR(Error, "Cannot reload the game module: no saved project is open");
    }

    return IsManagedProject(project->GetFilePath())
        ? ReloadManagedGameModule(project)
        : ReloadNativeGameModule(project);
}

EDITOR_API void UpdateGameModules(EditorSubsystem* subsystem)
{
    UpdateGameModules(subsystem->GetCurrentProject().Get(), subsystem->IsSimulating(), subsystem);
}

EDITOR_API bool HasRetiredNativeModules()
{
    Mutex::Guard guard(GetGameModulesMutex());

    return GetNativeModuleState().retiredModules.Any();
}

EDITOR_API void UpdateGameModules(EditorProject* project, bool isSimulating, EditorSubsystem* subsystem)
{
    NativeModuleState& state = GetNativeModuleState();

    const bool canReload = project != nullptr && project->IsSaved() && !isSimulating;

    String gameClassToCreate;
    Array<const Class*> classesToRebuild;

    // work that calls out of this file (game creation, node rebuilds, migrations) happens after the lock is released
    {
        Mutex::Guard guard(GetGameModulesMutex());

        if (isSimulating && project != nullptr && project->IsSaved())
        {
            if (LoadedNativeModule* playedModule = FindGameModule(state.loadedModules, GetNativeModulePath(project->GetFilePath())))
            {
                playedModule->flags |= NativeModuleFlags::HasPlayed;
            }
        }

        if (state.wasSimulating && !isSimulating)
        {
            for (RetiredNativeModule& retiredModule : state.retiredModules)
            {
                retiredModule.playsStoppedSinceRetired++;
            }
        }

        state.wasSimulating = isSimulating;

        if (canReload)
        {
            const FilePath modulePath = GetNativeModulePath(project->GetFilePath());

            LoadedNativeModule* loadedModule = FindGameModule(state.loadedModules, modulePath);

            if (loadedModule != nullptr && !(loadedModule->flags & NativeModuleFlags::WarnedRebuilt) && PollModuleRebuilt(*loadedModule))
            {
                TResult<LoadedNativeModule*> moduleResult = LoadLatestModule(modulePath, project->GetFilePath().BasePath());

                loadedModule = moduleResult.HasError() ? nullptr : moduleResult.GetValue();
            }

            if (loadedModule != nullptr && loadedModule->error.Empty())
            {
                gameClassToCreate = GetRetiredProjectGameReplacement(project, *loadedModule);
                classesToRebuild = std::move(state.classesToRebuild);

                state.classesToRebuild.Clear();
            }
        }
    }

    if (gameClassToCreate.Any())
    {
        ReplaceProjectGameWithClass(project, gameClassToCreate);
    }

    if (classesToRebuild.Any())
    {
        RebuildRetiredModuleObjects(project, subsystem, classesToRebuild);
    }

    if (canReload)
    {
        UpdateManagedGameModule(project);
    }

    bool hasRetiredModules = false;

    {
        Mutex::Guard guard(GetGameModulesMutex());

        hasRetiredModules = state.retiredModules.Any();
    }

    // the world syncs its own scenes each update; nothing else syncs the detached scene, where removed nodes are kept for undo
    if (hasRetiredModules)
    {
        GetDetachedSceneForCurrentThread().GetEntityManager()->SyncRuntimeComponentTypes();
    }

    Mutex::Guard guard(GetGameModulesMutex());

    UpdateRetiredModules();
}

} // namespace Hyperion
