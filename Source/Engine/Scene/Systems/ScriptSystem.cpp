/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Node.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Systems/ScriptSystem.hpp>

#include <Scripting/Asset/ScriptAsset.hpp>

#include <Scripting/ScriptObjectResource.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/AtomicVar.hpp>

#include <Core/Resource/Resource.hpp>

#include <DotNET/ManagedClass.hpp>
#include <DotNET/ManagedObject.hpp>
#include <DotNET/Assembly.hpp>
#include <DotNET/DotNETHost.hpp>

#include <Scripting/ScriptingService.hpp>
#include <Scripting/EntityScripting.hpp>
#include <Scripting/StrataTypes.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/AssetBucket.hpp>
#include <Asset/SerializationUtils.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/Property.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <System/DirectoryInitializer.hpp>

#include <Framework/Game.hpp>

#include <Plugins/PluginManager.hpp>

#include <HyperionEngine.hpp>

#include <ScriptSystem.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DEFINE_LOG_SUBCHANNEL(Scripting, Engine);

#ifdef HYP_EDITOR
static constexpr auto EnableScriptReloading = &EngineGlobals::IsEditor;
#else  // !HYP_EDITOR
static constexpr std::false_type EnableScriptReloading;
#endif // HYP_EDITOR

#pragma region ScriptTracker

class ScriptTracker
{
public:
    ScriptTracker()
    {
#ifdef HYP_DOTNET
        if (!DotNETHost::GetInstance().IsInitialized())
        {
            return;
        }

        SharedPtr<dotnet::Assembly> managedAssembly = DotNETHost::GetInstance().LoadAssembly("Hyperion.NET.Scripting.dll");
        Assert(managedAssembly != nullptr, "Failed to load Hyperion.NET.Scripting assembly");

        if (!managedAssembly)
        {
            return;
        }

        SharedPtr<dotnet::ManagedClass> managedClass = managedAssembly->FindClassByName("ScriptTracker");
        Assert(managedClass != nullptr, "Failed to load ScriptTracker class from Hyperion.NET.Scripting assembly (Guid: {})",
               managedAssembly->GetGuid());

        if (!managedClass)
        {
            return;
        }

        object = UniquePtr<dotnet::ManagedObject>(managedClass->NewObject());
        assembly = std::move(managedAssembly);
#endif // HYP_DOTNET
    }

    ~ScriptTracker()
    {
        Shutdown();
    }

    void Initialize(
        const Array<FilePath>& sourceDirectories,
        const FilePath& intermediateDirectory,
        const FilePath& binaryOutputDirectory,
        void (*callbackPtr)(void*, ScriptEvent),
        void* callbackSelfPtr)
    {
        if (!object || !object->IsValid())
        {
            return;
        }

        object->InvokeMethodByName<void>(
            "Initialize",
            sourceDirectories,
            intermediateDirectory,
            binaryOutputDirectory,
            reinterpret_cast<void*>(callbackPtr),
            callbackSelfPtr);
    }

    void InitializeCompiler(const FilePath& sourceDirectory, const FilePath& intermediateDirectory, const FilePath& binaryOutputDirectory)
    {
        if (!object || !object->IsValid())
        {
            return;
        }

        object->InvokeMethodByName<void>(
            "InitializeCompiler",
            sourceDirectory,
            intermediateDirectory,
            binaryOutputDirectory);
    }

    void UpdateSourceDirectories(const Array<FilePath>& sourceDirectories)
    {
        if (!object || !object->IsValid())
        {
            return;
        }

        object->InvokeMethodByName<void>(
            "UpdateSourceDirectories",
            sourceDirectories);
    }

    void SetReferenceAssembly(const FilePath& scopeDirectory, const FilePath& assemblyPath)
    {
        if (!object || !object->IsValid() || scopeDirectory.Empty())
        {
            return;
        }

        object->InvokeMethodByName<void>(
            "SetReferenceAssembly",
            scopeDirectory,
            assemblyPath);
    }

    void SetPluginModules(Span<const Name> moduleNames)
    {
        if (!object || !object->IsValid())
        {
            return;
        }

        Array<String> moduleNameStrings;

        for (Name moduleName : moduleNames)
        {
            moduleNameStrings.PushBack(moduleName.LookupString());
        }

        object->InvokeMethodByName<void>(
            "SetPluginModules",
            moduleNameStrings);
    }

    void RebuildModulesIn(const FilePath& directory)
    {
        if (!object || !object->IsValid())
        {
            return;
        }

        object->InvokeMethodByName<void>(
            "RebuildModulesIn",
            directory);
    }

    bool ResolveAssembly(const FilePath& scriptPath, ScriptDesc& scriptDesc)
    {
        if (!object || !object->IsValid())
        {
            return false;
        }

        return object->InvokeMethodByName<bool>(
            "ResolveAssembly",
            scriptPath,
            reinterpret_cast<void*>(&scriptDesc));
    }

    void InvokeUpdate()
    {
        if (!object || !object->IsValid())
        {
            return;
        }

        object->InvokeMethodByName<void>("Update");
    }

    void Shutdown()
    {
        if (!object || !object->IsValid())
        {
            return;
        }

        object->InvokeMethodByName<void>("Shutdown");

        object.Reset();
        assembly.Reset();
    }

    SharedPtr<dotnet::Assembly> assembly;
    UniquePtr<dotnet::ManagedObject> object;
};

#pragma endregion ScriptTracker

#pragma region Reloaded script classes

#ifdef HYP_DOTNET
static const Class* GetFirstStaticAncestor(const Class* cls)
{
    while (cls && cls->IsDynamic())
    {
        cls = cls->GetParent();
    }

    return cls;
}

static void RetargetDynamicObject(ObjectBase* object, const DynamicClassInstance* newClass)
{
    ObjectHeader* header = object->GetObjectHeader_Internal();
    const DynamicClassInstance* oldClass = static_cast<const DynamicClassInstance*>(header->cls);

    ScriptObjectResource* scriptObjectResource = object->GetScriptObjectResource();
    ScriptObjectData_DotNet* dotNetData = scriptObjectResource ? scriptObjectResource->GetScriptObjectData_DotNet() : nullptr;

    const SharedPtr<dotnet::ManagedClass> newManagedClass = newClass->GetManagedClass();

    if (!dotNetData || !newManagedClass)
    {
        return;
    }

    Array<Pair<Name, String>> carriedValues;

    {
        const BoxedValue target { MakeStrongRef(object) };

        for (const Class* cls = oldClass; cls && cls->IsDynamic(); cls = cls->GetParent())
        {
            for (const Property* property : cls->GetProperties())
            {
                if (!property->CanGet())
                {
                    continue;
                }

                String source;

                if (BoxedToHMF(property->Get(target), source, &property->GetTypeInfo()).HasError())
                {
                    continue;
                }

                carriedValues.PushBack({ property->GetName(), std::move(source) });
            }
        }
    }

    dotnet::ManagedObject* oldManagedObject = dotNetData->objectPtr;

    const_cast<DynamicClassInstance*>(newClass)->AddRef();
    header->cls = newClass;

    dotNetData->managedClass = newManagedClass;
    dotNetData->objectPtr = newManagedClass->NewObject(newClass, object);

    if (!dotNetData->objectPtr)
    {
        HYP_LOG(Scripting, Error, "Failed to create the reloaded managed object for {}", newClass->GetName());
    }
    else
    {
        int64 readers = 0;
        int64 writers = 0;
        scriptObjectResource->GetNumUsers(readers, writers);

        if (readers > 0)
        {
            dotNetData->objectPtr->SetKeepAlive(true);
        }
    }

    if (oldManagedObject)
    {
        // disposed now rather than finalized later, since the native object it refers to lives on
        const dotnet::ManagedMethod* disposeMethod = oldManagedObject->GetMethod("Dispose");

        if (!disposeMethod || !oldManagedObject->TryInvokeMethod<void>(disposeMethod, nullptr))
        {
            HYP_LOG(Scripting, Warning, "Couldn't dispose the managed object of {} replaced by a reload; it is left to the GC", oldClass->GetName());
        }

        oldManagedObject->SetKeepAlive(false);

        delete oldManagedObject;
    }

    const_cast<DynamicClassInstance*>(oldClass)->Release();

    BoxedValue target { MakeStrongRef(object) };

    for (const Pair<Name, String>& carriedValue : carriedValues)
    {
        const Property* property = newClass->GetProperty(carriedValue.first);

        if (!property || !property->CanSet())
        {
            continue;
        }

        TResult<BoxedValue> valueResult = BoxedAsType(BoxedValue(HMF::UnresolvedValue { carriedValue.second }), property->GetTypeInfo());

        if (valueResult.HasError())
        {
            HYP_LOG(Scripting, Warning, "Value of {}.{} doesn't fit its new type {}; it's reset", newClass->GetName(), carriedValue.first, property->GetTypeInfo().name);

            continue;
        }

        property->Set(target, valueResult.GetValue());
    }
}

static bool TryRetargetDynamicObject(ObjectBase* object, Name objectName)
{
    const Class* cls = object ? object->InstanceClass() : nullptr;

    if (!cls || !cls->IsDynamic())
    {
        return false;
    }

    const Class* newClass = GetClass(cls->GetTypeId());

    if (!newClass || newClass == cls || !newClass->IsDynamic())
    {
        return false;
    }

    if (GetFirstStaticAncestor(newClass) != GetFirstStaticAncestor(cls))
    {
        HYP_LOG(Scripting, Warning, "{} now derives from a different native class; reopen the project to load {} with it", newClass->GetName(), objectName);

        return false;
    }

    RetargetDynamicObject(object, static_cast<const DynamicClassInstance*>(newClass));

    return true;
}

#endif // HYP_DOTNET

void ScriptSystem::RetargetReloadedObjects(World& world)
{
#ifdef HYP_DOTNET
    uint32 numRetargeted = 0;

    for (Scene* scene : world.GetScenes())
    {
        if (!scene || !scene->GetRoot().IsValid())
        {
            continue;
        }

        for (Node* node : scene->GetRoot()->GetDescendantsArray())
        {
            if (node && TryRetargetDynamicObject(node, node->GetName()))
            {
                numRetargeted++;
            }
        }
    }

    if (Handle<AssetRegistry> assetRegistry = GetCurrentAssetRegistry(); assetRegistry.IsValid())
    {
        Array<Handle<AssetObject>> cachedAssets;
        assetRegistry->GetCachedAssets(cachedAssets);

        GlobalContextScope loadingContextScope { AssetLoadingContext {} };

        for (const Handle<AssetObject>& assetObject : cachedAssets)
        {
            if (TryRetargetDynamicObject(assetObject.Get(), assetObject->GetName()))
            {
                numRetargeted++;
            }
        }
    }

    if (numRetargeted != 0)
    {
        HYP_LOG(Scripting, Info, "Reloaded {} object(s) of C# classes", numRetargeted);
    }
#endif // HYP_DOTNET
}

#pragma endregion Reloaded script classes

#pragma region ScriptSystem

static const FilePath& GetScriptsSourceDirectory()
{
    static DirectoryInitializer<HYP_STATIC_STRING("Data/Scripts")> s_directory;
    return s_directory.path;
}

struct GameAssemblyReference
{
    FilePath projectDirectory;
    FilePath assemblyPath;
};

static Mutex s_gameAssemblyMutex;
static GameAssemblyReference s_gameAssembly;
static AtomicVar<uint32> s_gameAssemblyVersion { 0 };

static GameAssemblyReference GetGameAssemblyReference()
{
    Mutex::Guard guard(s_gameAssemblyMutex);

    return s_gameAssembly;
}

void ScriptSystem::SetGameAssembly(const FilePath& projectDirectory, const FilePath& assemblyPath, bool isNewBuild)
{
    {
        Mutex::Guard guard(s_gameAssemblyMutex);

        if (!isNewBuild && s_gameAssembly.projectDirectory == projectDirectory && s_gameAssembly.assemblyPath == assemblyPath)
        {
            return;
        }

        s_gameAssembly.projectDirectory = projectDirectory;
        s_gameAssembly.assemblyPath = assemblyPath;
    }

    s_gameAssemblyVersion.Increment(1, MemoryOrder::RELEASE);
}

void ScriptSystem::ApplyGameAssembly(bool rebuildOnReload)
{
    const uint32 version = s_gameAssemblyVersion.Get(MemoryOrder::ACQUIRE);

    if (!m_scriptTracker || version == m_gameAssemblyVersion)
    {
        return;
    }

    m_gameAssemblyVersion = version;

    const GameAssemblyReference gameAssembly = GetGameAssemblyReference();

    m_scriptTracker->SetReferenceAssembly(gameAssembly.projectDirectory, gameAssembly.assemblyPath);

    if (rebuildOnReload && gameAssembly.projectDirectory == m_gameProjectDirectory)
    {
        m_scriptTracker->RebuildModulesIn(gameAssembly.projectDirectory);
    }

    m_gameProjectDirectory = gameAssembly.projectDirectory;
}

ScriptSystem::ScriptSystem()
{
}

ScriptSystem::~ScriptSystem() = default;

bool ScriptSystem::AllowParallelExecution() const
{
    return false;
}

bool ScriptSystem::RequiresSimThread() const
{
    return true;
}

bool ScriptSystem::AllowUpdate() const
{
    return EnableScriptReloading();
}

void ScriptSystem::OnAddedToWorld(World* world)
{
    SystemBase::OnAddedToWorld(world);

    Game* gameInstance = world->GetGame();
    Assert(gameInstance != nullptr);

    m_delegateHandlers.Add(
        NAME("OnGameStateChange"),
        gameInstance->OnGameStateChange.Bind(
            gameInstance,
            [this](Game* gameInstance, GameStateMode previousGameStateMode, GameStateMode currentGameStateMode)
            {
                AssertOnThread(g_simThread);

                HandleGameStateChanged(currentGameStateMode, previousGameStateMode);
            }));

    if (EnableScriptReloading())
    {
        m_scriptingService = MakeUnique<ScriptingService>();

        m_delegateHandlers.Add(
            NAME("OnScriptStateChanged"),
            m_scriptingService->OnScriptStateChanged.Bind(
                [this](const ScriptDesc& inScriptDesc)
                {
                    AssertOnThread(g_simThread);

                    switch (inScriptDesc.language)
                    {
                    case ScriptLanguage::CSharp:
                        // Compilation is driven from C#
                        if (!(inScriptDesc.compileStatus & uint32(ScriptCompileStatus::Compiled)))
                        {
                            return;
                        }
                        break;
                    case ScriptLanguage::Strata:
                    {
#ifdef HYP_STRATA
                        // a module that only defines components has no entity script to reload, so re-register its types directly
                        const FilePath sourcePath(inScriptDesc.path.Data());
                        FilePath scriptsDirectory = sourcePath.BasePath();

                        for (FilePath directory = scriptsDirectory; !directory.Empty() && directory.BasePath() != directory; directory = directory.BasePath())
                        {
                            if (directory.Basename() == AssetBuckets::Scripts.GetName())
                            {
                                scriptsDirectory = directory;
                                break;
                            }
                        }

                        Strata::ReloadTypes(sourcePath, scriptsDirectory);
#endif // HYP_STRATA
                        break;
                    }
                    default:
                        break;
                    }

                    World* world = GetWorld();
                    Assert(world != nullptr);

                    if (!world)
                    {
                        return;
                    }

                    const GameState& gameState = world->GetGameState();

                    for (Scene* scene : world->GetScenes())
                    {
                        for (auto [entity, scriptComponent] : scene->GetEntityManager()->GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
                        {
                            const Handle<ScriptAsset>& scriptAsset = scriptComponent.script;

                            // e.g its script was deleted
                            if (!scriptAsset.IsValid())
                            {
                                continue;
                            }

                            auto writeScope = scriptAsset->GetWriteScope();

                            ScriptDesc& scriptDesc = scriptAsset->GetScriptDesc();

                            bool matchesScript = false;

                            if (inScriptDesc.language == ScriptLanguage::Strata
                                && scriptAsset->IsRegistered())
                            {
                                Handle<AssetRegistry> registry = scriptAsset->GetAssetRegistry();

                                if (registry.IsValid())
                                {
                                    const char* extension = ".strata";

                                    const FilePath incomingPath(inScriptDesc.path.Data());
                                    const FilePath expectedSourcePath = registry->GetRootPath() / "Scripts" / (scriptAsset->GetName().ToString() + extension);

                                    // Compare paths - handles both absolute and relative path forms
                                    if (incomingPath == expectedSourcePath
                                        || incomingPath.EndsWith(expectedSourcePath)
                                        || expectedSourcePath.EndsWith(incomingPath))
                                    {
                                        matchesScript = true;
                                    }
                                    else
                                    {
                                        HYP_LOG(Scripting, Verbose, "ScriptSystem: Path mismatch for {} script '{}' (incoming: '{}', expected: '{}')",
                                            uint32(inScriptDesc.language),
                                            scriptAsset->GetName().ToString(),
                                            incomingPath, expectedSourcePath);
                                    }
                                }
                            }
                            else
                            {
                                if (Memory::StrCmp(inScriptDesc.assemblyPath.Data(), scriptDesc.assemblyPath.Data(), MathUtil::Min(GetArrayCount(inScriptDesc.assemblyPath), GetArrayCount(scriptDesc.assemblyPath))) == 0)
                                {
                                    matchesScript = true;
                                }
                            }

                            if (matchesScript)
                            {
                                HYP_LOG(Scripting, Info, "ScriptSystem: Reloading script for entity {}", entity->Id());

                                scriptComponent.flags |= ScriptComponentFlags::RELOADING;

                                scriptDesc.uuid = inScriptDesc.uuid;
                                scriptDesc.compileStatus = inScriptDesc.compileStatus;
                                scriptDesc.hotReloadVersion = inScriptDesc.hotReloadVersion;
                                scriptDesc.lastModifiedTimestamp = inScriptDesc.lastModifiedTimestamp;

                                writeScope.Reset();

                                EntityScripting::ShutdownEntityScript(entity, scriptComponent, gameState);

                                scriptComponent.assembly.Reset();

                                EntityScripting::InitializeEntityScript(entity, scriptComponent, gameState);

                                scriptComponent.flags &= ~ScriptComponentFlags::RELOADING;

                                HYP_LOG(Scripting, Info, "ScriptSystem: Script reloaded for entity #{}", entity->Id());
                            }
                        }
                    }

#ifdef HYP_DOTNET
                    if (inScriptDesc.language == ScriptLanguage::CSharp)
                    {
                        DotNETHost::GetInstance().LoadAssembly(EntityScripting::GetCSharpAssemblyLoadPath(inScriptDesc).Data());

                        RetargetReloadedObjects(*world);
                    }
#endif // HYP_DOTNET

                    OnScriptReloaded();
                }));

        m_scriptTracker = MakeUnique<ScriptTracker>();
        m_scriptTracker->SetPluginModules(PluginManager::GetInstance().GetRuntimeModules(PluginManager::GetInstance().GetEnabledPlugins()));

        ApplyGameAssembly(/* rebuildOnReload */ false);

        void (*scriptTrackerCallback)(void*, ScriptEvent) = [](void* selfPtr, ScriptEvent event)
        {
            static_cast<ScriptingService*>(selfPtr)->PushScriptEvent(event);
        };

        m_scriptTracker->Initialize(
            CollectScriptSourceDirectories(),
            EngineGlobals::GetTempDirectory() / "ScriptProjects",
            CoreApi::GetExecutablePath(),
            scriptTrackerCallback,
            m_scriptingService.Get());
    }
}

Array<FilePath> ScriptSystem::CollectScriptSourceDirectories() const
{
    Array<FilePath> scriptSourceDirectories;
    scriptSourceDirectories.PushBack(GetScriptsSourceDirectory());

    if (Handle<AssetRegistry> assetRegistry = GetCurrentAssetRegistry(); assetRegistry.IsValid())
    {
        if (assetRegistry->GetRootPath().Exists())
        {
            const FilePath projectScriptsPath = assetRegistry->GetRootPath() / AssetBuckets::Scripts.GetName();

            scriptSourceDirectories.PushBack(projectScriptsPath);
        }
    }

    return scriptSourceDirectories;
}

Delegate<void> ScriptSystem::OnScriptReloaded;

SharedPtr<dotnet::Assembly> ScriptSystem::LoadProjectScriptAssembly(const FilePath& contentDirectory, Span<const Name> pluginModules)
{
#ifdef HYP_DOTNET
    if (!EnableScriptReloading())
    {
        return nullptr;
    }

    const FilePath scriptsDirectory = contentDirectory / AssetBuckets::Scripts.GetName();

    if (!scriptsDirectory.IsDirectory())
    {
        return nullptr;
    }

    bool hasCSharpScripts = false;

    for (const FilePath& file : scriptsDirectory.GetAllFilesInDirectory())
    {
        if (file.EndsWith(".cs"))
        {
            hasCSharpScripts = true;

            break;
        }
    }

    if (!hasCSharpScripts)
    {
        return nullptr;
    }

    ScriptTracker scriptTracker;

    // same source directory as the ScriptSystem's tracker, so the module (and its DLL) gets the same name
    scriptTracker.InitializeCompiler(
        GetScriptsSourceDirectory(),
        EngineGlobals::GetTempDirectory() / "ScriptProjects",
        CoreApi::GetExecutablePath());

    const GameAssemblyReference gameAssembly = GetGameAssemblyReference();
    scriptTracker.SetReferenceAssembly(gameAssembly.projectDirectory, gameAssembly.assemblyPath);
    scriptTracker.SetPluginModules(pluginModules);

    ScriptDesc scriptDesc {};
    scriptDesc.language = ScriptLanguage::CSharp;

    // only the directory of the script path is used to find the module
    if (!scriptTracker.ResolveAssembly(scriptsDirectory / "Module.cs", scriptDesc))
    {
        HYP_LOG(Scripting, Error, "Failed to build the C# scripts in {}; classes they declare won't load with the project's scenes", scriptsDirectory);

        return nullptr;
    }

    const ANSIString assemblyPath = EntityScripting::GetCSharpAssemblyLoadPath(scriptDesc);

    SharedPtr<dotnet::Assembly> assembly = DotNETHost::GetInstance().LoadAssembly(assemblyPath.Data());

    if (!assembly)
    {
        HYP_LOG(Scripting, Error, "Failed to load C# script assembly {}", assemblyPath.Data());

        return nullptr;
    }

    HYP_LOG(Scripting, Info, "Loaded C# script assembly {} for {}", assemblyPath.Data(), scriptsDirectory);

    return assembly;
#else
    return nullptr;
#endif
}

void ScriptSystem::RefreshScriptSourceDirectories()
{
    if (!EnableScriptReloading() || !m_scriptTracker)
    {
        return;
    }

    // Update the source dirs, may have changed, for example "Save As" project or first save going from temp dir -> actual concrete dir
    m_scriptTracker->UpdateSourceDirectories(CollectScriptSourceDirectories());
}

void ScriptSystem::OnRemovedFromWorld(World* world)
{
    SystemBase::OnRemovedFromWorld(world);

    m_delegateHandlers.Remove("OnGameStateChange"_sh);
    m_delegateHandlers.Remove("OnScriptStateChanged"_sh);

    if (EnableScriptReloading())
    {
        if (m_scriptTracker)
        {
            m_scriptTracker->Shutdown();
            m_scriptTracker.Reset();
        }

        if (m_scriptingService)
        {
            m_scriptingService.Reset();
        }
    }
}

void ScriptSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    ScriptComponent& scriptComponent = entity->GetEntityManager()->GetComponent<ScriptComponent>(entity);

    const GameState& gameState = GetWorld()->GetGameState();

    // the editor also creates (but doesn't activate) scripts while stopped, so their reflected fields can be edited
    if (!gameState.IsStopped() || EnableScriptReloading())
    {
        ResolveScriptAssembly(scriptComponent);

        EntityScripting::InitializeEntityScript(entity, scriptComponent, gameState);
    }
}

void ScriptSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    ScriptComponent& scriptComponent = entity->GetEntityManager()->GetComponent<ScriptComponent>(entity);

    const GameState& gameState = GetWorld()->GetGameState();

    EntityScripting::ShutdownEntityScript(entity, scriptComponent, gameState);
}

void ScriptSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    if (EnableScriptReloading())
    {
        if (m_scriptingService)
        {
            m_scriptingService->Update();
        }

        if (m_scriptTracker)
        {
            ApplyGameAssembly(/* rebuildOnReload */ true);

            m_scriptTracker->InvokeUpdate();
        }
    }
}

void ScriptSystem::RecreateEntityScript(Entity* entity)
{
    AssertOnThread(g_simThread);

    EntityManager* entityManager = entity->GetEntityManager();
    ScriptComponent* scriptComponent = entityManager ? entityManager->TryGetComponent<ScriptComponent>(entity) : nullptr;

    if (!scriptComponent)
    {
        return;
    }

    const GameState& gameState = GetWorld()->GetGameState();

    EntityScripting::ShutdownEntityScript(entity, *scriptComponent, gameState);

    scriptComponent->assembly.Reset();

    ResolveScriptAssembly(*scriptComponent);

    EntityScripting::InitializeEntityScript(entity, *scriptComponent, gameState);
}

void ScriptSystem::HandleGameStateChanged(GameStateMode gameStateMode, GameStateMode previousGameStateMode)
{
    World* world = GetWorld();

    if (!world)
    {
        return;
    }

    const GameState& gameState = world->GetGameState();

    if (gameStateMode == GameStateMode::STOPPED)
    {
        EntityScripting::QueryScriptedEntities(*world, [this, &gameState](Entity* entity, ScriptComponent& scriptComponent)
                                               {
                                                   EntityScripting::ShutdownEntityScript(entity, scriptComponent, gameState);

                                                   // back to an inactive editor instance
                                                   if (EnableScriptReloading())
                                                   {
                                                       ResolveScriptAssembly(scriptComponent);

                                                       EntityScripting::InitializeEntityScript(entity, scriptComponent, gameState);
                                                   }
                                               });

        return;
    }

    if (previousGameStateMode == GameStateMode::STOPPED)
    {
        EntityScripting::QueryScriptedEntities(*world, [this, &gameState](Entity* entity, ScriptComponent& scriptComponent)
                                               {
                                                   // play starts from a fresh object rather than the editor's inactive one
                                                   EntityScripting::ShutdownEntityScript(entity, scriptComponent, gameState);

                                                   ResolveScriptAssembly(scriptComponent);

                                                   EntityScripting::InitializeEntityScript(entity, scriptComponent, gameState);
                                               });

        return;
    }
}

void ScriptSystem::ResolveScriptAssembly(ScriptComponent& scriptComponent)
{
    if (!EnableScriptReloading() || !m_scriptTracker || scriptComponent.assembly)
    {
        return;
    }

    const Handle<ScriptAsset>& scriptAsset = scriptComponent.script;

    if (!scriptAsset || scriptAsset->GetScriptDesc().language != ScriptLanguage::CSharp || !scriptAsset->IsRegistered())
    {
        return;
    }

    Handle<AssetRegistry> registry = scriptAsset->GetAssetRegistry();

    if (!registry.IsValid())
    {
        return;
    }

    const FilePath sourcePath = registry->GetRootPath() / AssetBuckets::Scripts.GetName() / (scriptAsset->GetName().ToString() + ".cs");

    if (!sourcePath.Exists())
    {
        return;
    }

    auto writeScope = scriptAsset->GetWriteScope();

    ScriptDesc& scriptDesc = scriptAsset->GetScriptDesc();

    const String previousAssemblyPath = scriptDesc.SerializeAssemblyPath();

    if (!m_scriptTracker->ResolveAssembly(sourcePath, scriptDesc))
    {
        HYP_LOG(Scripting, Error, "ScriptSystem: Failed to build C# script '{}' from {}", scriptAsset->GetName(), sourcePath);

        return;
    }

    if (scriptDesc.SerializeAssemblyPath() != previousAssemblyPath)
    {
        writeScope.Reset();

        // persist it so builds without the script compiler can find the assembly
        scriptAsset->MarkDirty();
    }
}

#pragma endregion ScriptSystem

extern "C"
{
    // Setting a ScriptComponent's script doesn't go through OnEntityAdded, so the editor asks for the script object after setting it
    HYP_EXPORT void ScriptSystem_RecreateEntityScript(Entity* entity)
    {
        if (!entity || !entity->GetWorld())
        {
            return;
        }

        if (ScriptSystem* scriptSystem = entity->GetWorld()->GetSystem<ScriptSystem>())
        {
            scriptSystem->RecreateEntityScript(entity);
        }
    }
}

} // namespace Hyperion
