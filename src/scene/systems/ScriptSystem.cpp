/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/systems/ScriptSystem.hpp>
#include <scene/EntityManager.hpp>

#include <scene/World.hpp>

#include <scene/util/EntityScripting.hpp>

#include <asset/ScriptAsset.hpp>

#include <scripting/ScriptObjectResource.hpp>

#include <core/threading/Threads.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/DotNetSystem.hpp>

#include <scripting/ScriptingService.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#ifdef HYP_SCRIPT
#include <script/HypScript.hpp>

#endif

namespace hyperion {

constexpr bool g_enableScriptReloading = true;

template <class ReturnType, class... ArgTypes>
static void InvokeScriptMethodT(ReturnType* outReturnValue, ScriptObjectResource* sor, const char* methodName, ArgTypes&&... args)
{
    Assert(sor != nullptr);

    switch (sor->GetScriptLanguage())
    {
#ifdef HYP_DOTNET
    case SL_CSHARP:
    {
        AssertDebug(sor->GetManagedObject() != nullptr);

        if (dotnet::ManagedClass* classPtr = sor->GetManagedObject()->GetClass())
        {
            if (dotnet::ManagedMethod* methodPtr = classPtr->GetMethod(methodName))
            {
                if (methodPtr->GetAttributes().HasAttribute("ScriptMethodStub"))
                {
                    // Stubbed method, don't waste cycles calling it if it's not implemented

                    break;
                }

                if constexpr (!std::is_void_v<ReturnType>)
                {
                    AssertDebug(outReturnValue != nullptr);
                    new (outReturnValue) ReturnType(sor->GetManagedObject()->InvokeMethod<ReturnType>(methodPtr, std::forward<ArgTypes>(args)...));
                }
                else
                {
                    sor->GetManagedObject()->InvokeMethod<void>(methodPtr, std::forward<ArgTypes>(args)...);
                }
            }
        }

        break;
    }
#endif
#ifdef HYP_SCRIPT
    case SL_HYPSCRIPT:
    {
        auto* data = sor->GetScriptObjectData_HypScript();
        Assert(data != nullptr);

        HypScript& hs = HypScript::GetInstance();

        HypData functionValue;
        if (!hs.GetFunctionHandle(data->instance, methodName, functionValue))
        {
            break;
        }

        if (!IsFunction(functionValue))
        {
            break;
        }

        HypData returnValue = hs.CallFunction(data->instance, functionValue, std::forward<ArgTypes>(args)...);

        if constexpr (!std::is_void_v<ReturnType>)
        {
            Assert(returnValue.IsValid());
            Assert(returnValue.Is<ReturnType>());

            AssertDebug(outReturnValue != nullptr);

            new (outReturnValue) ReturnType(std::move(returnValue.Get<ReturnType>()));
        }

        break;
    }
#endif
    default:
        break;
    }
}

ScriptSystem::ScriptSystem(EntityManager& entityManager)
    : SystemBase(entityManager)
{
    // @FIXME: Issue with reloaded assemblies that spawn native objects having their classes change.

    if (g_enableScriptReloading)
    {
        m_delegateHandlers.Add(
            NAME("OnScriptStateChanged"),
            g_engineDriver->GetScriptingService()->OnScriptStateChanged.Bind([this](const ScriptData& script)
                {
                    Threads::AssertOnThread(g_gameThread);

                    if (!(script.compileStatus & uint32(SCS_COMPILED)))
                    {
                        return;
                    }

                    for (auto [entity, scriptComponent] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
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
    { // temp; removed for testing
      // return;
    }

    for (auto [entity, scriptComponent] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!(scriptComponent.flags & ScriptComponentFlags::INITIALIZED))
        {
            continue;
        }

        InvokeScriptMethodT<void>(nullptr, scriptComponent.scriptObjectResource, "Update", float(delta));
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

        InvokeScriptMethodT<void>(nullptr, scriptComponent.scriptObjectResource, *methodName);
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
