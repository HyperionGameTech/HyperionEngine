/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <scene/EntityManager.hpp>
#include <scene/systems/ScriptSystem.hpp>

#include <scene/util/EntityScripting.hpp>

#include <scripting/asset/ScriptAsset.hpp>

#include <scripting/ScriptObjectResource.hpp>

#include <Core/threading/Threads.hpp>

#include <Core/memory/resource/Resource.hpp>

#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/DotNETHost.hpp>

#include <scripting/ScriptingService.hpp>

#include <asset/AssetRegistry.hpp>

#include <system/DirectoryInitializer.hpp>

#include <engine/Game.hpp>

#include <HyperionEngine.hpp>

#ifdef HYP_SCRIPT
#include <Lang/HypScript.hpp>
#endif

#include <ScriptSystem.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DEFINE_LOG_SUBCHANNEL(Scripting, Engine);

#if HYP_EDITOR
constexpr bool EnableScriptReloading = true;
#else
constexpr bool EnableScriptReloading = false;
#endif

#pragma region ScriptTracker

class ScriptTracker
{
public:
    ScriptTracker()
    {
        // @TODO will this be an issue, if running from Editor?
        if (!DotNETHost::GetInstance().IsInitialized())
        {
            return;
        }

        RC<dotnet::Assembly> managedAssembly = DotNETHost::GetInstance().LoadAssembly("Hyperion.NET.Scripting.dll");
        Assert(managedAssembly != nullptr, "Failed to load Hyperion.NET.Scripting assembly");

        RC<dotnet::ManagedClass> managedClass = managedAssembly->FindClassByName("ScriptTracker");
        Assert(managedClass != nullptr, "Failed to load ScriptTracker class from Hyperion.NET.Scripting assembly (Guid: {})",
            managedAssembly->GetGuid());

        object = UniquePtr<dotnet::ManagedObject>(managedClass->NewObject());
        assembly = std::move(managedAssembly);
    }

    ~ScriptTracker()
    {
        Shutdown();
    }

    void Initialize(
        const Array<FilePath>& sourceDirectories,
        const FilePath& intermediateDirectory,
        const FilePath& binaryOutputDirectory,
        void* callbackPtr,
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
            callbackPtr,
            callbackSelfPtr);
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

    RC<dotnet::Assembly> assembly;
    UniquePtr<dotnet::ManagedObject> object;
};

#pragma endregion ScriptTracker

#pragma region ScriptSystem

static const FilePath& GetScriptsSourceDirectory()
{
    static DirectoryInitializer<HYP_STATIC_STRING("Data/Scripts"), /* RelativeToExecutablePath */ false> s_directory;
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
    return EnableScriptReloading;
}

void ScriptSystem::OnAddedToWorld(World* world)
{
    SystemBase::OnAddedToWorld(world);

    Game* gameInstance = world->GetGame();
    Assert(gameInstance != nullptr);

    m_delegateHandlers.Add(
        NAME("OnGameStateChange"),
        gameInstance->OnGameStateChange.Bind([this](Game* gameInstance, GameStateMode previousGameStateMode, GameStateMode currentGameStateMode)
            {
                AssertOnThread(g_simThread);

                HandleGameStateChanged(currentGameStateMode, previousGameStateMode);
            }));

    if constexpr (EnableScriptReloading)
    {
        m_scriptingService = MakeUnique<ScriptingService>();

        m_delegateHandlers.Add(
            NAME("OnScriptStateChanged"),
            m_scriptingService->OnScriptStateChanged.Bind([this](const ScriptDesc& inScriptDesc)
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
                    case ScriptLanguage::HypScript:
                        // Compilation is driven from C++
                        if (!(inScriptDesc.compileStatus & uint32(ScriptCompileStatus::Processing)))
                        {
                            return;
                        }
                        break;
                    default: break;
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

                            // @TODO: Will need `path` for hypscript, assemblypath is only relevent for c#.
                            if (Memory::StrCmp(inScriptDesc.assemblyPath.Data(), scriptDesc.assemblyPath.Data(), MathUtil::Min(ArraySize(inScriptDesc.assemblyPath), ArraySize(scriptDesc.assemblyPath))) == 0)
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

        Array<FilePath> scriptSourceDirectories;
        scriptSourceDirectories.PushBack(GetScriptsSourceDirectory());

        // Add project-specific scripts directory from asset registry
        if (Handle<AssetRegistry> assetRegistry = GetCurrentAssetRegistry(); assetRegistry.IsValid())
        {
            if (assetRegistry->GetRootPath().Exists())
            {
                const FilePath projectScriptsPath = assetRegistry->GetRootPath() / "Scripts";

                scriptSourceDirectories.PushBack(projectScriptsPath);
            }
        }

        m_scriptTracker->Initialize(
            scriptSourceDirectories,
            GetTempDirectory() / "ScriptProjects",
            CoreApi::GetExecutablePath(),
            reinterpret_cast<void*>(+[](void* selfPtr, ScriptEvent event)
            {
                static_cast<ScriptingService*>(selfPtr)->PushScriptEvent(event);
            }),
            m_scriptingService.Get()
        );
    }
}

void ScriptSystem::OnRemovedFromWorld(World* world)
{
    SystemBase::OnRemovedFromWorld(world);

    m_delegateHandlers.Remove("OnGameStateChange"_sh);
    m_delegateHandlers.Remove("OnScriptStateChanged"_sh);

    if constexpr (EnableScriptReloading)
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
    if constexpr (EnableScriptReloading)
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
        EntityScripting::QueryScriptedEntities(*world, [&gameState](Entity* entity, ScriptComponent& scriptComponent)
            {
                EntityScripting::InitializeEntityScript(entity, scriptComponent, gameState);
            });

        return;
    }
}

#pragma endregion ScriptSystem

} // namespace Hyperion
