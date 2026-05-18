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
#include <dotnet/DotNETHost.hpp>

#include <scripting/ScriptingService.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>
#include <engine/Game.hpp>

#ifdef HYP_SCRIPT
#include <Lang/HypScript.hpp>
#endif

#include <ScriptSystem.generated.inl>

namespace Hyperion {

EngineStatTimer g_statScriptUpdate("CPU/Script/Update");

#if HYP_EDITOR
constexpr bool EnableScriptReloading = true;
#else
constexpr bool EnableScriptReloading = false;
#endif

template <class ReturnType, class... ArgTypes>
static void InvokeScriptMethodT(ReturnType* outReturnValue, ScriptObjectResource* sor, const char* methodName, ArgTypes&&... args)
{
    Assert(sor != nullptr);

    const uint32 mask = sor->GetScriptLanguageMask();

#ifdef HYP_DOTNET
    if (mask & (1u << uint32(ScriptLanguage::CSharp)))
    {
        AssertDebug(sor->GetManagedObject() != nullptr);

        if (dotnet::ManagedClass* managedClass = sor->GetManagedObject()->GetClass())
        {
            if (dotnet::ManagedMethod* managedMethod = managedClass->GetMethod(methodName))
            {
                if (!managedMethod->GetAttributes().HasAttribute("ScriptMethodStub"))
                {
                    // Stubbed method, don't waste cycles calling it if it's not implemented
                    if constexpr (!std::is_void_v<ReturnType>)
                    {
                        AssertDebug(outReturnValue != nullptr);
                        new (outReturnValue) ReturnType(sor->GetManagedObject()->InvokeMethod<ReturnType>(managedMethod, std::forward<ArgTypes>(args)...));
                    }
                    else
                    {
                        sor->GetManagedObject()->InvokeMethod<void>(managedMethod, std::forward<ArgTypes>(args)...);
                    }
                }
            }
        }
    }
#endif
#ifdef HYP_SCRIPT
    if (mask & (1u << uint32(ScriptLanguage::HypScript)))
    {
        auto* data = sor->GetScriptObjectData_HypScript();
        Assert(data != nullptr);

        HypScript& hs = HypScript::GetInstance();

        BoxedValue functionValue;

        if (hs.GetFunctionHandle(data->instance, methodName, functionValue)
            && IsFunction(functionValue))
        {
            BoxedValue returnValue = hs.CallFunction(data->instance, functionValue, std::forward<ArgTypes>(args)...);

            if constexpr (!std::is_void_v<ReturnType>)
            {
                Assert(returnValue.IsValid());
                Assert(returnValue.Is<ReturnType>());

                AssertDebug(outReturnValue != nullptr);

                new (outReturnValue) ReturnType(std::move(returnValue.Get<ReturnType>()));
            }
        }
    }
#endif

    /// \todo add native script support here
}

ScriptSystem::ScriptSystem()
{
    // @FIXME: Issue with reloaded assemblies that spawn native objects having their classes change.

    if (EnableScriptReloading)
    {
        Assert(g_engineDriver->GetScriptingService() != nullptr);

        m_delegateHandlers.Add(
            NAME("OnScriptStateChanged"),
            g_engineDriver->GetScriptingService()->OnScriptStateChanged.Bind([this](const ScriptDesc& inScriptDesc)
                {
                    AssertOnThread(g_simThread);

                    switch (inScriptDesc.language)
                    {
                    case ScriptLanguage::CSharp:
                        // C# script recompilation is driven from C#
                        if (!(inScriptDesc.compileStatus & ScriptCompileStatus::Compiled))
                        {
                            return;
                        }
                        break;
                    case ScriptLanguage::HypScript:
                        // HypScript is compiled in C++, here. So we want it in Processing state if we're going to compile
                        if (!(inScriptDesc.compileStatus & ScriptCompileStatus::Processing))
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

                    static const auto Comparator = [](const ScriptDesc& inScriptDesc, const ScriptDesc& scriptDesc) -> bool
                    {
                        switch (inScriptDesc.language)
                        {
                        case ScriptLanguage::CSharp:
                            return Memory::StrCmp(
                                       inScriptDesc.assemblyPath.Data(),
                                       scriptDesc.assemblyPath.Data(),
                                       MathUtil::Min(inScriptDesc.assemblyPath.Size(), scriptDesc.assemblyPath.Size()))
                                == 0;
                        case ScriptLanguage::HypScript:
                            // @TODO We should save deps for hypscript and can check if we need to recomopile based on that.
                            // If we do that, we'll need to sort which files get compiled first + Have a different way to compile
                            // directly rather than abusing (De)initEntityScriptComponent
                            return Memory::StrCmp(
                                       inScriptDesc.path.Data(),
                                       scriptDesc.path.Data(),
                                       MathUtil::Min(inScriptDesc.path.Size(), scriptDesc.path.Size()))
                                == 0;
                        default:
                            return false;
                        }
                    };

                    for (Scene* scene : world->GetScenes())
                    {
                        for (auto [entity, scriptComponent] : scene->GetEntityManager()->GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
                        {
                            const Handle<ScriptAsset>& scriptAsset = scriptComponent.script;
                            Assert(scriptAsset != nullptr);

                            auto resGuard = scriptAsset->GetReadScope();

                            ScriptDesc& scriptDesc = scriptAsset->GetScriptDesc();

                            if (Comparator(inScriptDesc, scriptDesc))
                            {
                                HYP_LOG(Script, Info, "ScriptSystem: Reloading script for entity #{}", entity->Id());

                                // Reload the script
                                scriptComponent.flags |= ScriptComponentFlags::RELOADING;

                                scriptDesc.uuid = inScriptDesc.uuid;
                                scriptDesc.compileStatus = inScriptDesc.compileStatus;
                                scriptDesc.hotReloadVersion = inScriptDesc.hotReloadVersion;
                                scriptDesc.lastModifiedTimestamp = inScriptDesc.lastModifiedTimestamp;

                                // Release read scope - may need recompilation which will need exclusive access.
                                resGuard.Reset();

                                EntityScripting::DeinitEntityScriptComponent(entity, scriptComponent);

                                scriptComponent.assembly.Reset();

                                EntityScripting::InitEntityScriptComponent(entity, scriptComponent);

                                scriptComponent.flags &= ~ScriptComponentFlags::RELOADING;

                                HYP_LOG(Script, Info, "ScriptSystem: Script reloaded for entity #{}", entity->Id());
                            }
                        }
                    }
                }));
    }
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
}

void ScriptSystem::OnRemovedFromWorld(World* world)
{
    SystemBase::OnRemovedFromWorld(world);

    m_delegateHandlers.Remove("OnGameStateChange"_sh);
}

void ScriptSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    ScriptComponent& scriptComponent = entity->GetEntityManager()->GetComponent<ScriptComponent>(entity);

    EntityScripting::InitEntityScriptComponent(entity, scriptComponent);
}

void ScriptSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    ScriptComponent& scriptComponent = entity->GetEntityManager()->GetComponent<ScriptComponent>(entity);

    EntityScripting::DeinitEntityScriptComponent(entity, scriptComponent);
}

void ScriptSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    World* world = GetWorld();

    if (!world)
    {
        return;
    }

    // Only update scripts if we're in simulation mode
    if (world->GetGameState().mode != GameStateMode::SIMULATING)
    {
        return;
    }

    ENGINE_STAT_SCOPE(&g_statScriptUpdate);

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, scriptComponent] : scene->GetEntityManager()->GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
        {
            if (!(scriptComponent.flags & ScriptComponentFlags::INITIALIZED))
            {
                continue;
            }

            InvokeScriptMethodT<void>(nullptr, scriptComponent.scriptObjectResource, "Update", float(delta));
        }
    }
}

void ScriptSystem::HandleGameStateChanged(GameStateMode gameStateMode, GameStateMode previousGameStateMode)
{
    if (previousGameStateMode == GameStateMode::SIMULATING)
    {
        CallScriptMethod("OnPlayStop");
    }

    if (gameStateMode == GameStateMode::SIMULATING)
    {
        CallScriptMethod("OnPlayStart");
    }
}

void ScriptSystem::CallScriptMethod(UTF8StringView methodName)
{
    World* world = GetWorld();
    AssertDebug(world != nullptr);

    if (!world)
    {
        return;
    }

    for (Scene* scene : world->GetScenes())
    {
        for (auto [entity, scriptComponent] : scene->GetEntityManager()->GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
        {
            if (!(scriptComponent.flags & ScriptComponentFlags::INITIALIZED))
            {
                continue;
            }

            InvokeScriptMethodT<void>(nullptr, scriptComponent.scriptObjectResource, *methodName);
        }
    }
}

void ScriptSystem::CallScriptMethod(UTF8StringView methodName, ScriptComponent& target)
{
    if (!(target.flags & ScriptComponentFlags::INITIALIZED))
    {
        return;
    }

    InvokeScriptMethodT<void>(nullptr, target.scriptObjectResource, *methodName);
}

} // namespace Hyperion
