/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/ScriptSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/World.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/DotNetSystem.hpp>
#include <dotnet/runtime/scene/ManagedSceneTypes.hpp>

#include <scripting/ScriptingService.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

ScriptSystem::ScriptSystem(EntityManager &entity_manager)
    : System(entity_manager)
{
    m_delegate_handlers.Add(NAME("OnScriptStateChanged"), g_engine->GetScriptingService()->OnScriptStateChanged.Bind([this](const ManagedScript &script)
    {
        Threads::AssertOnThread(ThreadName::THREAD_GAME);

        if (!(script.state & uint32(CompiledScriptState::COMPILED))) {
            return;
        }

        for (auto [entity, script_component] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos())) {
            if (ANSIStringView(script.assembly_path) == ANSIStringView(script_component.script.assembly_path)) {
                HYP_LOG(Script, Info, "ScriptSystem: Reloading script for entity #{}", entity.Value());

                // Reload the script
                script_component.flags |= ScriptComponentFlags::RELOADING;

                script_component.script.uuid = script.uuid;
                script_component.script.state = script.state;
                script_component.script.hot_reload_version = script.hot_reload_version;
                script_component.script.last_modified_timestamp = script.last_modified_timestamp;

                OnEntityRemoved(entity);
                OnEntityAdded(Handle<Entity>(entity));

                script_component.flags &= ~ScriptComponentFlags::RELOADING;

                HYP_LOG(Script, Info, "ScriptSystem: Script reloaded for entity #{}", entity.Value());
            }
        }
    }));

    if (World *world = GetWorld()) {
        m_delegate_handlers.Add(NAME("OnGameStateChange"), world->OnGameStateChange.Bind([this](World *world, GameStateMode game_state_mode)
        {
            Threads::AssertOnThread(ThreadName::THREAD_GAME);

            const GameStateMode previous_game_state_mode = world->GetGameState().mode;

            HandleGameStateChanged(game_state_mode, previous_game_state_mode);
        }));
    }

    m_delegate_handlers.Add(NAME("OnWorldChange"), OnWorldChanged.Bind([this](World *new_world, World *previous_world)
    {
        Threads::AssertOnThread(ThreadName::THREAD_GAME);

        // Remove previous OnGameStateChange handler
        m_delegate_handlers.Remove(NAME("OnGameStateChange"));

        // If we were simulating before we need to stop it
        if (previous_world != nullptr && previous_world->GetGameState().mode == GameStateMode::SIMULATING) {
            CallScriptMethod("OnPlayStop");
        }

        if (new_world != nullptr) {
            // If the newly set world is simulating we need to notify the scripts
            if (new_world->GetGameState().mode == GameStateMode::SIMULATING) {
                CallScriptMethod("OnPlayStart");
            }

            // Add new handler for the new world's game state changing
            m_delegate_handlers.Add(NAME("OnGameStateChange"), new_world->OnGameStateChange.Bind([this](World *world, GameStateMode game_state_mode)
            {
                Threads::AssertOnThread(ThreadName::THREAD_GAME);

                const GameStateMode previous_game_state_mode = world->GetGameState().mode;

                HandleGameStateChanged(game_state_mode, previous_game_state_mode);
            }));
        }
    }));
}

void ScriptSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    ScriptComponent &script_component = GetEntityManager().GetComponent<ScriptComponent>(entity);

    if (script_component.flags & ScriptComponentFlags::INITIALIZED) {
        if (GetWorld()->GetGameState().mode == GameStateMode::SIMULATING) {
            CallScriptMethod("OnPlayStart", script_component);
        }

        return;
    }

    script_component.assembly = nullptr;
    script_component.object.Reset();

    ANSIString assembly_path(script_component.script.assembly_path);

    if (script_component.script.hot_reload_version > 0) {
        // @FIXME Implement FindLastIndex
        const SizeType extension_index = assembly_path.FindFirstIndex(".dll");

        if (extension_index != ANSIString::not_found) {
            assembly_path = assembly_path.Substr(0, extension_index)
                + "." + ANSIString::ToString(script_component.script.hot_reload_version)
                + ".dll";
        } else {
            assembly_path = assembly_path
                + "." + ANSIString::ToString(script_component.script.hot_reload_version)
                + ".dll";
        }
    }

    if (UniquePtr<dotnet::Assembly> assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly(assembly_path.Data())) {
        if (dotnet::Class *class_ptr = assembly->GetClassObjectHolder().FindClassByName(script_component.script.class_name)) {
            HYP_LOG(Script, Info, "ScriptSystem::OnEntityAdded: Loaded class '{}' from assembly '{}'", script_component.script.class_name, script_component.script.assembly_path);

            if (!class_ptr->HasParentClass("Script")) {
                HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Class '{}' from assembly '{}' does not inherit from 'Script'", script_component.script.class_name, script_component.script.assembly_path);

                return;
            }

            script_component.object = class_ptr->NewObject();

            if (!(script_component.flags & ScriptComponentFlags::BEFORE_INIT_CALLED)) {
                if (dotnet::Method *before_init_method_ptr = class_ptr->GetMethod("BeforeInit")) {
                    HYP_NAMED_SCOPE("Call BeforeInit() on script component");
                    HYP_LOG(Script, Info, "Calling BeforeInit() on script component");

                    AssertThrow(GetEntityManager().GetScene() != nullptr);
                    AssertThrow(GetEntityManager().GetScene()->GetWorld() != nullptr);

                    script_component.object.InvokeMethod<void>(
                        before_init_method_ptr,
                        GetWorld(),
                        GetScene()
                    );

                    script_component.flags |= ScriptComponentFlags::BEFORE_INIT_CALLED;
                }
            }

            if (!(script_component.flags & ScriptComponentFlags::INIT_CALLED)) {
                if (dotnet::Method *init_method_ptr = class_ptr->GetMethod("Init")) {
                    HYP_NAMED_SCOPE("Call Init() on script component");
                    HYP_LOG(Script, Info, "Calling Init() on script component");

                    script_component.object.InvokeMethod<void>(init_method_ptr, entity);

                    script_component.flags |= ScriptComponentFlags::INIT_CALLED;
                }
            }
        }

        script_component.assembly = std::move(assembly);
    }

    if (!script_component.assembly) {
        HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to load assembly '{}'", script_component.script.assembly_path);

        return;
    }

    if (!script_component.object.IsValid()) {
        HYP_LOG(Script, Error, "ScriptSystem::OnEntityAdded: Failed to create object of class '{}' from assembly '{}'", script_component.script.class_name, script_component.script.assembly_path);

        return;
    }

    script_component.flags |= ScriptComponentFlags::INITIALIZED;

    // Call OnPlayStart on first init if we're currently simulating
    if (GetWorld()->GetGameState().mode == GameStateMode::SIMULATING) {
        CallScriptMethod("OnPlayStart", script_component);
    }
}

void ScriptSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    ScriptComponent &script_component = GetEntityManager().GetComponent<ScriptComponent>(entity);

    if (!(script_component.flags & ScriptComponentFlags::INITIALIZED)) {
        return;

    }
    // If we're simulating while the script is removed, call OnPlayStop so OnPlayStart never gets double called
    if (GetWorld()->GetGameState().mode == GameStateMode::SIMULATING) {
        CallScriptMethod("OnPlayStop", script_component);
    }

    if (script_component.object.IsValid()) {
        if (dotnet::Class *class_ptr = script_component.object.GetClass()) {
            if (class_ptr->HasMethod("Destroy")) {
                HYP_NAMED_SCOPE("Call Destroy() on script component");

                script_component.object.InvokeMethodByName<void>("Destroy");
            }
        }
    }

    script_component.object.Reset();
    script_component.assembly = nullptr;

    script_component.flags &= ~(ScriptComponentFlags::INITIALIZED | ScriptComponentFlags::BEFORE_INIT_CALLED | ScriptComponentFlags::INIT_CALLED);
}

void ScriptSystem::Process(GameCounter::TickUnit delta)
{
    // Only update scripts if we're in simulation mode
    if (GetWorld()->GetGameState().mode != GameStateMode::SIMULATING) {
        return;
    }

    for (auto [entity_id, script_component] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos())) {
        if (!(script_component.flags & ScriptComponentFlags::INITIALIZED)) {
            continue;
        }

        if (dotnet::Class *class_ptr = script_component.object.GetClass()) {
            if (dotnet::Method *update_method_ptr = class_ptr->GetMethod("Update")) {
                if (update_method_ptr->GetAttributes().HasAttribute("ScriptMethodStub")) {
                    // Stubbed method, don't waste cycles calling it if it's not implemented
                    continue;
                }

                HYP_NAMED_SCOPE("Call Update() on script component");

                script_component.object.InvokeMethod<void, float>(update_method_ptr, float(delta));
            }
        }
    }
}

void ScriptSystem::HandleGameStateChanged(GameStateMode game_state_mode, GameStateMode previous_game_state_mode)
{
    HYP_SCOPE;

    if (previous_game_state_mode == GameStateMode::SIMULATING) {
        CallScriptMethod("OnPlayStop");
    }

    if (game_state_mode == GameStateMode::SIMULATING) {
        CallScriptMethod("OnPlayStart");
    }
}

void ScriptSystem::CallScriptMethod(UTF8StringView method_name)
{
    for (auto [entity_id, script_component] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos())) {
        if (!(script_component.flags & ScriptComponentFlags::INITIALIZED)) {
            continue;
        }

        if (dotnet::Class *class_ptr = script_component.object.GetClass()) {
            if (dotnet::Method *method_ptr = class_ptr->GetMethod(method_name)) {
                if (method_ptr->GetAttributes().HasAttribute("ScriptMethodStub")) {
                    // Stubbed method, don't waste cycles calling it if it's not implemented
                    continue;
                }

                script_component.object.InvokeMethod<void>(method_ptr);
            }
        }
    }
}

void ScriptSystem::CallScriptMethod(UTF8StringView method_name, ScriptComponent &target)
{
    if (!(target.flags & ScriptComponentFlags::INITIALIZED)) {
        return;
    }

    if (dotnet::Class *class_ptr = target.object.GetClass()) {
        if (dotnet::Method *method_ptr = class_ptr->GetMethod(method_name)) {
            if (method_ptr->GetAttributes().HasAttribute("ScriptMethodStub")) {
                // Stubbed method, don't waste cycles calling it if it's not implemented
                return;
            }

            target.object.InvokeMethod<void>(method_ptr);
        }
    }
}

} // namespace hyperion
