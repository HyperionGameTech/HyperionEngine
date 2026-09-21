/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Systems/ScriptSystem.hpp>

#include <Scripting/Asset/ScriptAsset.hpp>

#include <Scripting/ScriptObjectResource.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Resource/Resource.hpp>

#include <DotNET/ManagedClass.hpp>
#include <DotNET/ManagedObject.hpp>
#include <DotNET/Assembly.hpp>
#include <DotNET/DotNETHost.hpp>

#include <Scripting/ScriptingService.hpp>
#include <Scripting/EntityScripting.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetBucket.hpp>

#include <System/DirectoryInitializer.hpp>

#include <Framework/Game.hpp>

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

#pragma region ScriptSystem

static const FilePath& GetScriptsSourceDirectory()
{
    static DirectoryInitializer<HYP_STATIC_STRING("Data/Scripts")> s_directory;
    return s_directory.path;
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
                        break;
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
                            Assert(scriptAsset != nullptr);

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
                }));

        m_scriptTracker = MakeUnique<ScriptTracker>();

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

void ScriptSystem::RefreshScriptSourceDirectories()
{
    if (!EnableScriptReloading() || !m_scriptTracker)
    {
        return;
    }

    // Update the source dirs, may have changed, for example "Save As" project or first save going from temp dir -> actual concrete dir
    m_scriptTracker->UpdateSourceDirectories(CollectScriptSourceDirectories());
}

void ScriptSystem::PreloadProjectScripts(const Handle<AssetRegistry>& registry)
{
#ifdef HYP_DOTNET
    if (!registry.IsValid() || !DotNETHost::GetInstance().IsInitialized())
    {
        return;
    }

    if (!EnableScriptReloading())
    {
        Array<AssetDesc> scriptAssetDescs;
        registry->GetBucketAssetDescs(AssetBuckets::Scripts.GetIndex(), scriptAssetDescs);

        Array<ANSIString> loadedAssemblyPaths;

        for (const AssetDesc& scriptAssetDesc : scriptAssetDescs)
        {
            Handle<ScriptAsset> scriptAsset = registry->GetAsset<ScriptAsset>(AssetBuckets::Scripts, scriptAssetDesc.name);

            if (!scriptAsset.IsValid())
            {
                continue;
            }

            auto readScope = scriptAsset->GetReadScope();

            const ScriptDesc& scriptDesc = scriptAsset->GetScriptDesc();

            if (scriptDesc.language != ScriptLanguage::CSharp || scriptDesc.assemblyPath[0] == '\0')
            {
                continue;
            }

            // several scripts share a module
            const ANSIString assemblyPath = EntityScripting::GetCSharpAssemblyLoadPath(scriptDesc);

            if (loadedAssemblyPaths.Contains(assemblyPath))
            {
                continue;
            }

            loadedAssemblyPaths.PushBack(assemblyPath);

            if (!DotNETHost::GetInstance().LoadAssembly(assemblyPath.Data()))
            {
                HYP_LOG(Scripting, Error, "ScriptSystem: Failed to load C# script module '{}'", assemblyPath.Data());
            }
        }

        return;
    }

    const FilePath scriptsDirectory = registry->GetRootPath() / AssetBuckets::Scripts.GetName();

    if (!scriptsDirectory.IsDirectory())
    {
        return;
    }

    // same compiler settings as the tracker in OnAddedToWorld(), so modules resolve to the same names and outputs
    ScriptTracker scriptTracker;
    scriptTracker.InitializeCompiler(GetScriptsSourceDirectory(), EngineGlobals::GetTempDirectory() / "ScriptProjects", CoreApi::GetExecutablePath());

    Array<FilePath> pendingDirectories;
    pendingDirectories.PushBack(scriptsDirectory);

    while (pendingDirectories.Any())
    {
        const FilePath directory = pendingDirectories.PopBack();

        for (const FilePath& subdirectory : directory.GetSubdirectories())
        {
            pendingDirectories.PushBack(subdirectory);
        }

        // every C# source in a directory builds into the same module, so resolving one of them loads all of them
        for (const FilePath& file : directory.GetAllFilesInDirectory())
        {
            if (!file.EndsWith(".cs"))
            {
                continue;
            }

            ScriptDesc scriptDesc;
            scriptDesc.language = ScriptLanguage::CSharp;

            if (!scriptTracker.ResolveAssembly(file, scriptDesc))
            {
                HYP_LOG(Scripting, Error, "ScriptSystem: Failed to build C# scripts in {}; components they define won't be available until they build", directory);

                break;
            }

            const ANSIString assemblyPath = EntityScripting::GetCSharpAssemblyLoadPath(scriptDesc);

            if (!DotNETHost::GetInstance().LoadAssembly(assemblyPath.Data()))
            {
                HYP_LOG(Scripting, Error, "ScriptSystem: Failed to load C# script module '{}'", assemblyPath.Data());
            }

            break;
        }
    }
#endif // HYP_DOTNET
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

    if (!gameState.IsStopped())
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
            m_scriptTracker->InvokeUpdate();
        }
    }
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
        EntityScripting::QueryScriptedEntities(*world, [&gameState](Entity* entity, ScriptComponent& scriptComponent)
                                               {
                                                   EntityScripting::ShutdownEntityScript(entity, scriptComponent, gameState);
                                               });

        return;
    }

    if (previousGameStateMode == GameStateMode::STOPPED)
    {
        EntityScripting::QueryScriptedEntities(*world, [this, &gameState](Entity* entity, ScriptComponent& scriptComponent)
                                               {
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

} // namespace Hyperion
