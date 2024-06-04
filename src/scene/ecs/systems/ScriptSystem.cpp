/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/ScriptSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/DotNetSystem.hpp>
#include <dotnet/runtime/scene/ManagedSceneTypes.hpp>
#include <dotnet/runtime/ManagedHandle.hpp>

#include <scripting/ScriptingService.hpp>

#include <Engine.hpp>

namespace hyperion {

ScriptSystem::ScriptSystem(EntityManager &entity_manager)
    : System(entity_manager)
{
    m_delegate_handlers.Add(g_engine->GetScriptingService()->OnScriptStateChanged.Bind([this](const ManagedScript &script)
    {
        Threads::AssertOnThread(ThreadName::THREAD_GAME);

        HYP_LOG(Script, LogLevel::INFO, "ScriptSystem: Detected script state change for script '{}' (assembly: {}, class: {}, state: {})",
            script.uuid, script.assembly_path, script.class_name, script.state);

        if (!(script.state & uint32(CompiledScriptState::COMPILED))) {
            return;
        }

        for (auto [entity, script_component] : GetEntityManager().GetEntitySet<ScriptComponent>()) {
            if (ANSIStringView(script.assembly_path) == ANSIStringView(script_component.script.assembly_path)) {
                HYP_LOG(Script, LogLevel::INFO, "ScriptSystem: Reloading script for entity #{} (assembly {}, class: {})",
                    entity.Value(), script_component.script.assembly_path, script_component.script.class_name);

                // Reload the script
                script_component.flags |= ScriptComponentFlags::RELOADING;

                OnEntityRemoved(entity);
                OnEntityAdded(entity);

                script_component.flags &= ~ScriptComponentFlags::RELOADING;

                HYP_LOG(Script, LogLevel::INFO, "ScriptSystem: Script reloaded for entity #{}", entity.Value());
            }
        }
    }));
}

void ScriptSystem::OnEntityAdded(ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity);

    ScriptComponent &script_component = GetEntityManager().GetComponent<ScriptComponent>(entity);

    if (script_component.flags & ScriptComponentFlags::INITIALIZED) {
        return;
    }

    script_component.assembly = nullptr;
    script_component.object = nullptr;

    if (RC<dotnet::Assembly> managed_assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly(script_component.script.assembly_path)) {
        if (dotnet::Class *class_ptr = managed_assembly->GetClassObjectHolder().FindClassByName(script_component.script.class_name)) {
            script_component.object = class_ptr->NewObject();

            if (auto *before_init_method_ptr = class_ptr->GetMethod("BeforeInit")) {
                script_component.object->InvokeMethod<void, ManagedHandle>(
                    before_init_method_ptr,
                    CreateManagedHandleFromID(GetEntityManager().GetScene()->GetID())
                );
            }

            if (auto *init_method_ptr = class_ptr->GetMethod("Init")) {
                script_component.object->InvokeMethod<void, ManagedEntity>(
                    init_method_ptr,
                    ManagedEntity { entity.Value() }
                );
            }
        }

        script_component.assembly = std::move(managed_assembly);
    }

    if (!script_component.assembly) {
        HYP_LOG(Script, LogLevel::ERROR, "ScriptSystem::OnEntityAdded: Failed to load assembly '{}'", script_component.script.assembly_path);

        return;
    }

    if (!script_component.object) {
        HYP_LOG(Script, LogLevel::ERROR, "ScriptSystem::OnEntityAdded: Failed to create object of class '{}' from assembly '{}'", script_component.script.class_name, script_component.script.assembly_path);

        return;
    }

    script_component.flags |= ScriptComponentFlags::INITIALIZED;
}

void ScriptSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    ScriptComponent &script_component = GetEntityManager().GetComponent<ScriptComponent>(entity);

    if (!(script_component.flags & ScriptComponentFlags::INITIALIZED)) {
        return;
    }

    if (script_component.object != nullptr) {
        if (dotnet::Class *class_ptr = script_component.object->GetClass()) {
            if (class_ptr->HasMethod("Destroy")) {
                script_component.object->InvokeMethodByName<void>("Destroy");
            }
        }
    }

    script_component.object = nullptr;
    script_component.assembly = nullptr;

    script_component.flags &= ~ScriptComponentFlags::INITIALIZED;
}

void ScriptSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity_id, script_component] : GetEntityManager().GetEntitySet<ScriptComponent>()) {
        if (!(script_component.flags & ScriptComponentFlags::INITIALIZED)) {
            continue;
        }

        if (dotnet::Class *class_ptr = script_component.object->GetClass()) {

            if (auto *update_method_ptr = class_ptr->GetMethod("Update")) {
                if (update_method_ptr->HasAttribute("Hyperion.ScriptMethodStub")) {
                    // Stubbed method, don't waste cycles calling it if it's not implemented
                    continue;
                }

                HYP_LOG(Script, LogLevel::DEBUG, "ScriptSystem::Process: Calling Update on entity {}", entity_id.Value());

                script_component.object->InvokeMethod<void, float>(update_method_ptr, float(delta));
            }
        }
    }
}

} // namespace hyperion
