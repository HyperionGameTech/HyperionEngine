/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <scene/EntityManager.hpp>
#include <scene/systems/ScriptSystem.hpp>

#include <scene/util/EntityScripting.hpp>

#include <asset/ScriptAsset.hpp>

#include <scripting/ScriptObjectResource.hpp>

#include <core/threading/Threads.hpp>

#include <core/memory/resource/Resource.hpp>

#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/DotNETHost.hpp>

#include <scripting/ScriptingService.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>
#include <engine/Game.hpp>

#ifdef HYP_SCRIPT
#include <script/HypScript.hpp>
#endif

#include <ScriptSystem.generated.inl>

namespace hyperion {

EngineStatTimer g_scriptUpdateTimer("Script/Update");

constexpr bool g_enableScriptReloading = false;

template <class ReturnType, class... ArgTypes>
static void InvokeScriptMethodT(ReturnType* outReturnValue, ScriptObjectResource* sor, const char* methodName, ArgTypes&&... args)
{
    Assert(sor != nullptr);

    const uint32 mask = sor->GetScriptLanguageMask();

#ifdef HYP_DOTNET
    if (mask & (1u << SL_CSHARP))
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
    if (mask & (1u << SL_HYPSCRIPT))
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

    if (g_enableScriptReloading)
    {
        m_delegateHandlers.Add(
            NAME("OnScriptStateChanged"),
            g_engineDriver->GetScriptingService()->OnScriptStateChanged.Bind([this](const ScriptData& script)
                {
                    AssertOnThread(g_simThread);

                    if (!(script.compileStatus & uint32(SCS_COMPILED)))
                    {
                        return;
                    }

                    World* world = GetWorld();
                    Assert(world != nullptr);

                    if (!world)
                    {
                        return;
                    }

                    for (Scene* scene : world->GetScenes())
                    {
                        for (auto [entity, scriptComponent] : scene->GetEntityManager()->GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
                        {
                            const Handle<ScriptAsset>& scriptAsset = scriptComponent.assetReference.Resolve();
                            Assert(scriptAsset != nullptr);

                            ResourceHandle resourceHandle(*scriptAsset->GetResource());

                            ScriptData* scriptData = scriptAsset->GetScriptData();
                            Assert(scriptData != nullptr);

                            if (Memory::StrCmp(script.assemblyPath.Data(), scriptData->assemblyPath.Data(), MathUtil::Min(ArraySize(script.assemblyPath), ArraySize(scriptData->assemblyPath))) == 0)
                            {
                                HYP_LOG(Script, Info, "ScriptSystem: Reloading script for entity #{}", entity->Id());

                                // Reload the script
                                scriptComponent.flags |= ScriptComponentFlags::RELOADING;

                                scriptData->uuid = script.uuid;
                                scriptData->compileStatus = script.compileStatus;
                                scriptData->hotReloadVersion = script.hotReloadVersion;
                                scriptData->lastModifiedTimestamp = script.lastModifiedTimestamp;

                                resourceHandle.Reset();

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

    if (World* world = GetWorld())
    {
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

    // m_delegateHandlers.Add(
    //     NAME("OnWorldChange"),
    //     OnWorldChanged.Bind([this](World* newWorld, World* previousWorld)
    //         {
    //             AssertOnThread(g_simThread);

    //             // Remove previous OnGameStateChange handler
    //             m_delegateHandlers.Remove(NAME("OnGameStateChange"));

    //             // If we were simulating before we need to stop it
    //             if (previousWorld != nullptr && previousWorld->GetGameState().mode == GameStateMode::SIMULATING)
    //             {
    //                 CallScriptMethod("OnPlayStop");
    //             }

    //             if (newWorld != nullptr)
    //             {
    //                 // If the newly set world is simulating we need to notify the scripts
    //                 if (newWorld->GetGameState().mode == GameStateMode::SIMULATING)
    //                 {
    //                     CallScriptMethod("OnPlayStart");
    //                 }

    //                 // Add new handler for the new world's game state changing
    //                 m_delegateHandlers.Add(
    //                     NAME("OnGameStateChange"),
    //                     newWorld->OnGameStateChange.Bind([this](World* world, GameStateMode gameStateMode)
    //                         {
    //                             AssertOnThread(g_simThread);

    //                             const GameStateMode previousGameStateMode = world->GetGameState().mode;

    //                             HandleGameStateChanged(gameStateMode, previousGameStateMode);
    //                         }));
    //             }
    //         }));
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

    ENGINE_STAT_SCOPE(&g_scriptUpdateTimer);

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
    HYP_SCOPE;

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

} // namespace hyperion
