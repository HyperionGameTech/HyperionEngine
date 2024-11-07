/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/ScriptSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/DotNetSystem.hpp>
#include <dotnet/runtime/scene/ManagedSceneTypes.hpp>

#include <scripting/ScriptingService.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

ScriptSystem::ScriptSystem(EntityManager &entity_manager)
    : System(entity_manager)
{
    m_delegate_handlers.Add(g_engine->GetScriptingService()->OnScriptStateChanged.Bind([this](const ManagedScript &script)
    {
        Threads::AssertOnThread(ThreadName::THREAD_GAME);

        if (!(script.state & uint32(CompiledScriptState::COMPILED))) {
            return;
        }

        for (auto [entity, script_component] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos())) {
            if (ANSIStringView(script.assembly_path) == ANSIStringView(script_component.script.assembly_path)) {
                HYP_LOG(Script, LogLevel::INFO, "ScriptSystem: Reloading script for entity #{}", entity.Value());

                // Reload the script
                script_component.flags |= ScriptComponentFlags::RELOADING;

                script_component.script.uuid = script.uuid;
                script_component.script.state = script.state;
                script_component.script.hot_reload_version = script.hot_reload_version;
                script_component.script.last_modified_timestamp = script.last_modified_timestamp;

                OnEntityRemoved(entity);
                OnEntityAdded(Handle<Entity>(entity));

                script_component.flags &= ~ScriptComponentFlags::RELOADING;

                HYP_LOG(Script, LogLevel::INFO, "ScriptSystem: Script reloaded for entity #{}", entity.Value());
            }
        }
    }));
}

void ScriptSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    ScriptComponent &script_component = GetEntityManager().GetComponent<ScriptComponent>(entity);

    if (script_component.flags & ScriptComponentFlags::INITIALIZED) {
        return;
    }

    script_component.assembly = nullptr;
    script_component.object = nullptr;

    ANSIString assembly_path(script_component.script.assembly_path);

    if (script_component.script.hot_reload_version > 0) {
        // @FIXME Implement FindLastIndex
        const SizeType extension_index = assembly_path.FindIndex(".dll");

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
            HYP_LOG(Script, LogLevel::INFO, "ScriptSystem::OnEntityAdded: Loaded class '{}' from assembly '{}'", script_component.script.class_name, script_component.script.assembly_path);

            if (!class_ptr->HasParentClass("Script")) {
                HYP_LOG(Script, LogLevel::ERR, "ScriptSystem::OnEntityAdded: Class '{}' from assembly '{}' does not inherit from 'Script'", script_component.script.class_name, script_component.script.assembly_path);

                return;
            }

            script_component.object = class_ptr->NewObject();

            if (!(script_component.flags & ScriptComponentFlags::BEFORE_INIT_CALLED)) {
                if (dotnet::Method *before_init_method_ptr = class_ptr->GetMethod("BeforeInit")) {
                    HYP_NAMED_SCOPE("Call BeforeInit() on script component");
                    HYP_LOG(Script, LogLevel::INFO, "Calling BeforeInit() on script component");

                    AssertThrow(GetEntityManager().GetScene() != nullptr);
                    AssertThrow(GetEntityManager().GetScene()->GetWorld() != nullptr);

                    script_component.object->InvokeMethod<void, dotnet::Object *>(
                        before_init_method_ptr,
                        GetEntityManager().GetScene()->GetManagedObject()
                    );

                    script_component.flags |= ScriptComponentFlags::BEFORE_INIT_CALLED;
                }
            }

            if (!(script_component.flags & ScriptComponentFlags::INIT_CALLED)) {
                if (dotnet::Method *init_method_ptr = class_ptr->GetMethod("Init")) {
                    HYP_NAMED_SCOPE("Call Init() on script component");
                    HYP_LOG(Script, LogLevel::INFO, "Calling Init() on script component");

                    script_component.object->InvokeMethod<void, dotnet::Object *>(
                        init_method_ptr,
                        entity->GetManagedObject()
                    );

                    script_component.flags |= ScriptComponentFlags::INIT_CALLED;
                }
            }
        }

        script_component.assembly = std::move(assembly);
    }

    if (!script_component.assembly) {
        HYP_LOG(Script, LogLevel::ERR, "ScriptSystem::OnEntityAdded: Failed to load assembly '{}'", script_component.script.assembly_path);

        return;
    }

    if (!script_component.object) {
        HYP_LOG(Script, LogLevel::ERR, "ScriptSystem::OnEntityAdded: Failed to create object of class '{}' from assembly '{}'", script_component.script.class_name, script_component.script.assembly_path);

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
                HYP_NAMED_SCOPE("Call Destroy() on script component");

                script_component.object->InvokeMethodByName<void>("Destroy");
            }
        }
    }

    script_component.object = nullptr;
    script_component.assembly = nullptr;

    script_component.flags &= ~(ScriptComponentFlags::INITIALIZED | ScriptComponentFlags::BEFORE_INIT_CALLED | ScriptComponentFlags::INIT_CALLED);
}

void ScriptSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity_id, script_component] : GetEntityManager().GetEntitySet<ScriptComponent>().GetScopedView(GetComponentInfos())) {
        if (!(script_component.flags & ScriptComponentFlags::INITIALIZED)) {
            continue;
        }

        if (dotnet::Class *class_ptr = script_component.object->GetClass()) {
            if (dotnet::Method *update_method_ptr = class_ptr->GetMethod("Update")) {
                if (update_method_ptr->GetAttributes().HasAttribute("ScriptMethodStub")) {
                    // Stubbed method, don't waste cycles calling it if it's not implemented
                    continue;
                }

                HYP_NAMED_SCOPE("Call Update() on script component");

                script_component.object->InvokeMethod<void, float>(update_method_ptr, float(delta));
            }
        }
    }
}

} // namespace hyperion
