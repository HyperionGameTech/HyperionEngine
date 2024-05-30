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

#include <Engine.hpp>

namespace hyperion {

void ScriptSystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity_manager, entity);

    ScriptComponent &script_component = entity_manager.GetComponent<ScriptComponent>(entity);

    if (script_component.flags & SCF_INIT) {
        return;
    }

    script_component.assembly = nullptr;
    script_component.object = nullptr;

    if (RC<dotnet::Assembly> managed_assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly(script_component.script_info.assembly_name.Data())) {
        if (dotnet::Class *class_ptr = managed_assembly->GetClassObjectHolder().FindClassByName(script_component.script_info.class_name.Data())) {
            script_component.object = class_ptr->NewObject();

            if (auto *before_init_method_ptr = class_ptr->GetMethod("BeforeInit")) {
                script_component.object->InvokeMethod<void, ManagedHandle>(
                    before_init_method_ptr,
                    CreateManagedHandleFromID(entity_manager.GetScene()->GetID())
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
        HYP_LOG(Script, LogLevel::ERR, "ScriptSystem::OnEntityAdded: Failed to load assembly '{}'", script_component.script_info.assembly_name);

        return;
    }

    if (!script_component.object) {
        HYP_LOG(Script, LogLevel::ERR, "ScriptSystem::OnEntityAdded: Failed to create object of class '{}' from assembly '{}'", script_component.script_info.class_name, script_component.script_info.assembly_name.Data());

        return;
    }

    script_component.flags |= SCF_INIT;
}

void ScriptSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity_manager, entity);

    ScriptComponent &script_component = entity_manager.GetComponent<ScriptComponent>(entity);

    if (!(script_component.flags & SCF_INIT)) {
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

    script_component.flags &= ~SCF_INIT;
}

void ScriptSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    for (auto [entity_id, script_component] : entity_manager.GetEntitySet<ScriptComponent>()) {
        if (!(script_component.flags & SCF_INIT)) {
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
