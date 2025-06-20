/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/ScriptSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/World.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/DotNetSystem.hpp>

#include <scripting/ScriptingService.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

constexpr bool g_enable_script_reloading = true;

ScriptSystem::ScriptSystem(EntityManager& entity_manager)
    : SystemBase(entity_manager)
{
    // @FIXME: Issue with reloaded assemblies that spawn native objects having their classes change.

    if (g_enable_script_reloading)
    {
        m_delegate_handlers.Add(
            NAME("OnScriptStateChanged"),
            g_engine->GetScriptingService()->OnScriptStateChanged.Bind([this](const ManagedScript& script)
                {
                    Threads::AssertOnThread(g_game_thread);

                    if (!(script.state & uint32(CompiledScriptState::COMPILED)))
                    {
                        return;
                    }

                    for (auto [entity, script_component] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
                    {
                        if (ANSIStringView(script.assembly_path) == ANSIStringView(script_component.script.assembly_path))
                        {
                            HYP_LOG(Script, Info, "ScriptSystem: Reloading script for entity #{}", entity->GetID());

                            // Reload the script
                            script_component.flags |= ScriptComponentFlags::RELOADING;

                            script_component.script.uuid = script.uuid;
                            script_component.script.state = script.state;
                            script_component.script.hot_reload_version = script.hot_reload_version;
                            script_component.script.last_modified_timestamp = script.last_modified_timestamp;

                            OnEntityRemoved(entity);

                            script_component.assembly.Reset();

                            OnEntityAdded(Handle<Entity>(entity));

                            script_component.flags &= ~ScriptComponentFlags::RELOADING;

                            HYP_LOG(Script, Info, "ScriptSystem: Script reloaded for entity #{}", entity->GetID());
                        }
                    }
                }));
    }

    if (World* world = GetWorld())
    {
        m_delegate_handlers.Add(
            NAME("OnGameStateChange"),
            world->OnGameStateChange.Bind([this](World* world, GameStateMode previous_game_state_mode, GameStateMode current_game_state_mode)
                {
                    Threads::AssertOnThread(g_game_thread);

                    HandleGameStateChanged(current_game_state_mode, previous_game_state_mode);
                }));
    }

    // m_delegate_handlers.Add(
    //     NAME("OnWorldChange"),
    //     OnWorldChanged.Bind([this](World* new_world, World* previous_world)
    //         {
    //             Threads::AssertOnThread(g_game_thread);

    //             // Remove previous OnGameStateChange handler
    //             m_delegate_handlers.Remove(NAME("OnGameStateChange"));

    //             // If we were simulating before we need to stop it
    //             if (previous_world != nullptr && previous_world->GetGameState().mode == GameStateMode::SIMULATING)
    //             {
    //                 CallScriptMethod("OnPlayStop");
    //             }

    //             if (new_world != nullptr)
    //             {
    //                 // If the newly set world is simulating we need to notify the scripts
    //                 if (new_world->GetGameState().mode == GameStateMode::SIMULATING)
    //                 {
    //                     CallScriptMethod("OnPlayStart");
    //                 }

    //                 // Add new handler for the new world's game state changing
    //                 m_delegate_handlers.Add(
    //                     NAME("OnGameStateChange"),
    //                     new_world->OnGameStateChange.Bind([this](World* world, GameStateMode game_state_mode)
    //                         {
    //                             Threads::AssertOnThread(g_game_thread);

    //                             const GameStateMode previous_game_state_mode = world->GetGameState().mode;

    //                             HandleGameStateChanged(game_state_mode, previous_game_state_mode);
    //                         }));
    //             }
    //         }));
}

void ScriptSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    World* world = GetWorld();
    ScriptComponent& script_component = GetEntityManager().GetComponent<ScriptComponent>(entity);

    if (script_component.flags & ScriptComponentFlags::INITIALIZED)
    {
        if (world != nullptr && world->GetGameState().mode == GameStateMode::SIMULATING)
        {
            CallScriptMethod("OnPlayStart", script_component);
        }

        return;
    }

    if (!script_component.resource || !script_component.resource->GetManagedObject() || !script_component.resource->GetManagedObject()->IsValid())
    {
        FreeResource<ManagedObjectResource>(script_component.resource);
        script_component.resource = nullptr;

        if (!script_component.assembly)
        {
            ANSIString assembly_path(script_component.script.assembly_path);

            if (script_component.script.hot_reload_version > 0)
            {
                // @FIXME Implement FindLastIndex
                const SizeType extension_index = assembly_path.FindFirstIndex(".dll");

                if (extension_index != ANSIString::not_found)
                {
                    assembly_path = assembly_path.Substr(0, extension_index)
                        + "." + ANSIString::ToString(script_component.script.hot_reload_version)
                        + ".dll";
                }
                else
                {
                    assembly_path = assembly_path
                        + "." + ANSIString::ToString(script_component.script.hot_reload_version)
                        + ".dll";
                }
            }

            if (RC<dotnet::Assembly> assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly(assembly_path.Data()))
            {
                script_component.assembly = std::move(assembly);
            }
            else
            {
                HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to load assembly '{}'", script_component.script.assembly_path);

                return;
            }
        }

        if (RC<dotnet::Class> class_ptr = script_component.assembly->FindClassByName(script_component.script.class_name))
        {
            HYP_LOG(Script, Info, "ScriptSystem::OnEntityAdded: Loaded class '{}' from assembly '{}'", script_component.script.class_name, script_component.script.assembly_path);

            if (!class_ptr->HasParentClass("Script"))
            {
                HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Class '{}' from assembly '{}' does not inherit from 'Script'", script_component.script.class_name, script_component.script.assembly_path);

                return;
            }

            dotnet::Object* object = class_ptr->NewObject();
            AssertThrow(object != nullptr);

            script_component.resource = AllocateResource<ManagedObjectResource>(object, class_ptr);
            script_component.resource->IncRef();

            if (!(script_component.flags & ScriptComponentFlags::BEFORE_INIT_CALLED))
            {
                if (dotnet::Method* before_init_method_ptr = class_ptr->GetMethod("BeforeInit"))
                {
                    HYP_NAMED_SCOPE("Call BeforeInit() on script component");
                    HYP_LOG(Script, Debug, "Calling BeforeInit() on script component");

                    object->InvokeMethod<void>(before_init_method_ptr, GetWorld(), GetScene());

                    script_component.flags |= ScriptComponentFlags::BEFORE_INIT_CALLED;
                }
            }

            if (!(script_component.flags & ScriptComponentFlags::INIT_CALLED))
            {
                if (dotnet::Method* init_method_ptr = class_ptr->GetMethod("Init"))
                {
                    HYP_NAMED_SCOPE("Call Init() on script component");
                    HYP_LOG(Script, Info, "Calling Init() on script component");

                    object->InvokeMethod<void>(init_method_ptr, entity);

                    script_component.flags |= ScriptComponentFlags::INIT_CALLED;
                }
            }
        }

        if (!script_component.resource || !script_component.resource->GetManagedObject() || !script_component.resource->GetManagedObject()->IsValid())
        {
            HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to create object of class '{}' from assembly '{}'", script_component.script.class_name, script_component.script.assembly_path);

            if (script_component.resource)
            {
                script_component.resource->DecRef();

                FreeResource<ManagedObjectResource>(script_component.resource);
                script_component.resource = nullptr;
            }

            return;
        }
    }

    script_component.flags |= ScriptComponentFlags::INITIALIZED;

    // Call OnPlayStart on first init if we're currently simulating
    if (world != nullptr && world->GetGameState().mode == GameStateMode::SIMULATING)
    {
        CallScriptMethod("OnPlayStart", script_component);
    }
}

void ScriptSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    World* world = GetWorld();

    ScriptComponent& script_component = GetEntityManager().GetComponent<ScriptComponent>(entity);

    if (!(script_component.flags & ScriptComponentFlags::INITIALIZED))
    {
        return;
    }

    // If we're simulating while the script is removed, call OnPlayStop so OnPlayStart never gets double called
    if (world != nullptr && world->GetGameState().mode == GameStateMode::SIMULATING)
    {
        CallScriptMethod("OnPlayStop", script_component);
    }

    if (script_component.resource)
    {
        if (script_component.resource->GetManagedObject() && script_component.resource->GetManagedObject()->IsValid())
        {
            if (dotnet::Class* class_ptr = script_component.resource->GetManagedObject()->GetClass())
            {
                if (class_ptr->HasMethod("Destroy"))
                {
                    HYP_NAMED_SCOPE("Call Destroy() on script component");

                    script_component.resource->GetManagedObject()->InvokeMethodByName<void>("Destroy");
                }
            }
        }

        script_component.resource->DecRef();

        FreeResource<ManagedObjectResource>(script_component.resource);
        script_component.resource = nullptr;
    }

    script_component.flags &= ~(ScriptComponentFlags::INITIALIZED | ScriptComponentFlags::BEFORE_INIT_CALLED | ScriptComponentFlags::INIT_CALLED);
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

    for (auto [entity, script_component] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!(script_component.flags & ScriptComponentFlags::INITIALIZED))
        {
            continue;
        }

        AssertDebug(script_component.resource != nullptr);
        AssertDebug(script_component.resource->GetManagedObject() != nullptr);

        if (dotnet::Class* class_ptr = script_component.resource->GetManagedObject()->GetClass())
        {
            if (dotnet::Method* update_method_ptr = class_ptr->GetMethod("Update"))
            {
                if (update_method_ptr->GetAttributes().HasAttribute("ScriptMethodStub"))
                {
                    // Stubbed method, don't waste cycles calling it if it's not implemented
                    continue;
                }

                HYP_NAMED_SCOPE("Call Update() on script component");

                script_component.resource->GetManagedObject()->InvokeMethod<void, float>(update_method_ptr, float(delta));
            }
        }
    }
}

void ScriptSystem::HandleGameStateChanged(GameStateMode game_state_mode, GameStateMode previous_game_state_mode)
{
    HYP_SCOPE;

    if (previous_game_state_mode == GameStateMode::SIMULATING)
    {
        CallScriptMethod("OnPlayStop");
    }

    if (game_state_mode == GameStateMode::SIMULATING)
    {
        CallScriptMethod("OnPlayStart");
    }
}

void ScriptSystem::CallScriptMethod(UTF8StringView method_name)
{
    for (auto [entity, script_component] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!(script_component.flags & ScriptComponentFlags::INITIALIZED))
        {
            continue;
        }

        AssertDebug(script_component.resource != nullptr);
        AssertDebug(script_component.resource->GetManagedObject() != nullptr);

        if (dotnet::Class* class_ptr = script_component.resource->GetManagedObject()->GetClass())
        {
            if (dotnet::Method* method_ptr = class_ptr->GetMethod(method_name))
            {
                if (method_ptr->GetAttributes().HasAttribute("ScriptMethodStub"))
                {
                    // Stubbed method, don't waste cycles calling it if it's not implemented
                    continue;
                }

                script_component.resource->GetManagedObject()->InvokeMethod<void>(method_ptr);
            }
        }
    }
}

void ScriptSystem::CallScriptMethod(UTF8StringView method_name, ScriptComponent& target)
{
    if (!(target.flags & ScriptComponentFlags::INITIALIZED))
    {
        return;
    }

    AssertDebug(target.resource != nullptr);
    AssertDebug(target.resource->GetManagedObject() != nullptr);

    if (dotnet::Class* class_ptr = target.resource->GetManagedObject()->GetClass())
    {
        if (dotnet::Method* method_ptr = class_ptr->GetMethod(method_name))
        {
            if (method_ptr->GetAttributes().HasAttribute("ScriptMethodStub"))
            {
                // Stubbed method, don't waste cycles calling it if it's not implemented
                return;
            }

            target.resource->GetManagedObject()->InvokeMethod<void>(method_ptr);
        }
    }
}

} // namespace hyperion
