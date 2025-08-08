/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/systems/ScriptSystem.hpp>
#include <scene/EntityManager.hpp>

#include <scene/World.hpp>

#include <scene/util/EntityScripting.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/DotNetSystem.hpp>

#include <scripting/ScriptingService.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

constexpr bool g_enableScriptReloading = true;

ScriptSystem::ScriptSystem(EntityManager& entityManager)
    : SystemBase(entityManager)
{
    // @FIXME: Issue with reloaded assemblies that spawn native objects having their classes change.

    if (g_enableScriptReloading)
    {
        m_delegateHandlers.Add(
            NAME("OnScriptStateChanged"),
            g_engineDriver->GetScriptingService()->OnScriptStateChanged.Bind([this](const ManagedScript& script)
                {
                    Threads::AssertOnThread(g_gameThread);

                    if (!(script.compileStatus & uint32(SCS_COMPILED)))
                    {
                        return;
                    }

                    for (auto [entity, scriptComponent] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
                    {
                        if (ANSIStringView(script.assemblyPath) == ANSIStringView(scriptComponent.script.assemblyPath))
                        {
                            HYP_LOG(Script, Info, "ScriptSystem: Reloading script for entity #{}", entity->Id());

                            // Reload the script
                            scriptComponent.flags |= ScriptComponentFlags::RELOADING;

                            scriptComponent.script.uuid = script.uuid;
                            scriptComponent.script.compileStatus = script.compileStatus;
                            scriptComponent.script.hotReloadVersion = script.hotReloadVersion;
                            scriptComponent.script.lastModifiedTimestamp = script.lastModifiedTimestamp;

                            EntityScripting::DeinitEntityScriptComponent(entity, scriptComponent);

                            scriptComponent.assembly.Reset();

                            EntityScripting::InitEntityScriptComponent(entity, scriptComponent);

                            scriptComponent.flags &= ~ScriptComponentFlags::RELOADING;

                            HYP_LOG(Script, Info, "ScriptSystem: Script reloaded for entity #{}", entity->Id());
                        }
                    }
                }));
    }

    if (World* world = GetWorld())
    {
        m_delegateHandlers.Add(
            NAME("OnGameStateChange"),
            world->OnGameStateChange.Bind([this](World* world, GameStateMode previousGameStateMode, GameStateMode currentGameStateMode)
                {
                    Threads::AssertOnThread(g_gameThread);

                    HandleGameStateChanged(currentGameStateMode, previousGameStateMode);
                }));
    }

    // m_delegateHandlers.Add(
    //     NAME("OnWorldChange"),
    //     OnWorldChanged.Bind([this](World* newWorld, World* previousWorld)
    //         {
    //             Threads::AssertOnThread(g_gameThread);

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
    //                             Threads::AssertOnThread(g_gameThread);

    //                             const GameStateMode previousGameStateMode = world->GetGameState().mode;

    //                             HandleGameStateChanged(gameStateMode, previousGameStateMode);
    //                         }));
    //             }
    //         }));
}

void ScriptSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    ScriptComponent& scriptComponent = GetEntityManager().GetComponent<ScriptComponent>(entity);

    EntityScripting::InitEntityScriptComponent(entity, scriptComponent);
}

void ScriptSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    ScriptComponent& scriptComponent = GetEntityManager().GetComponent<ScriptComponent>(entity);

    EntityScripting::DeinitEntityScriptComponent(entity, scriptComponent);
}

void ScriptSystem::Process(float delta)
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

    for (auto [entity, scriptComponent] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!(scriptComponent.flags & ScriptComponentFlags::INITIALIZED))
        {
            continue;
        }

        AssertDebug(scriptComponent.resource != nullptr);
        AssertDebug(scriptComponent.resource->GetManagedObject() != nullptr);

        if (dotnet::Class* classPtr = scriptComponent.resource->GetManagedObject()->GetClass())
        {
            if (dotnet::Method* updateMethodPtr = classPtr->GetMethod("Update"))
            {
                if (updateMethodPtr->GetAttributes().HasAttribute("ScriptMethodStub"))
                {
                    // Stubbed method, don't waste cycles calling it if it's not implemented
                    continue;
                }

                HYP_NAMED_SCOPE("Call Update() on script component");

                scriptComponent.resource->GetManagedObject()->InvokeMethod<void, float>(updateMethodPtr, float(delta));
            }
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
    for (auto [entity, scriptComponent] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!(scriptComponent.flags & ScriptComponentFlags::INITIALIZED))
        {
            continue;
        }

        AssertDebug(scriptComponent.resource != nullptr);
        AssertDebug(scriptComponent.resource->GetManagedObject() != nullptr);

        if (dotnet::Class* classPtr = scriptComponent.resource->GetManagedObject()->GetClass())
        {
            if (dotnet::Method* methodPtr = classPtr->GetMethod(methodName))
            {
                if (methodPtr->GetAttributes().HasAttribute("ScriptMethodStub"))
                {
                    // Stubbed method, don't waste cycles calling it if it's not implemented
                    continue;
                }

                scriptComponent.resource->GetManagedObject()->InvokeMethod<void>(methodPtr);
            }
        }
    }
}

void ScriptSystem::CallScriptMethod(UTF8StringView methodName, ScriptComponent& target)
{
    if (!(target.flags & ScriptComponentFlags::INITIALIZED))
    {
        return;
    }

    AssertDebug(target.resource != nullptr);
    AssertDebug(target.resource->GetManagedObject() != nullptr);

    if (dotnet::Class* classPtr = target.resource->GetManagedObject()->GetClass())
    {
        if (dotnet::Method* methodPtr = classPtr->GetMethod(methodName))
        {
            if (methodPtr->GetAttributes().HasAttribute("ScriptMethodStub"))
            {
                // Stubbed method, don't waste cycles calling it if it's not implemented
                return;
            }

            target.resource->GetManagedObject()->InvokeMethod<void>(methodPtr);
        }
    }
}

} // namespace hyperion
