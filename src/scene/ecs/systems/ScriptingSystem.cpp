#include <scene/ecs/systems/ScriptingSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

void ScriptingSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    // @TODO: parallelize this system. will need to make sure the system itself is not in a task thread.

    for (auto [entity_id, script_component] : entity_manager.GetEntitySet<ScriptComponent>()) {
        Handle<Script> &script = script_component.script;
        
        if (!script) {
            continue;
        }

        if (!(script_component.flags & SCRIPT_COMPONENT_FLAG_INIT)) {
            InitObject(script);

            script_component.flags |= SCRIPT_COMPONENT_FLAG_INIT;

            if (script->Compile()) {
                script->Bake();
                script->Run(); // Run the script once to initialize the global state

                // If the script has a target object, setup our handles
                if (script_component.target_name.Length()) {
                    // Setup our self object
                    if (!script->GetObjectHandle(script_component.target_name.Data(), script_component.target_object)) {
                        DebugLog(
                            LogType::Error,
                            "Failed to get object handle for target object '%s'\n",
                            script_component.target_name.Data()
                        );

                        continue;
                    }

                    // Setup our script method handles
                    script->GetMember(script_component.target_object, "OnAdded", script_component.script_methods[SCRIPT_METHOD_ON_ADDED]);
                    script->GetMember(script_component.target_object, "OnRemoved", script_component.script_methods[SCRIPT_METHOD_ON_REMOVED]);
                    script->GetMember(script_component.target_object, "OnTick", script_component.script_methods[SCRIPT_METHOD_ON_TICK]);
                }
            }

            // Made it here, script is valid
            script_component.flags |= SCRIPT_COMPONENT_FLAG_VALID;

            if (!script_component.script_methods[SCRIPT_METHOD_ON_ADDED].IsNull()) {
                script->CallFunction(
                    script_component.script_methods[SCRIPT_METHOD_ON_ADDED],
                    script_component.target_object,
                    script->CreateInternedObject<Handle<Scene>>(Handle<Scene>(entity_manager.GetScene()->GetID())),
                    entity_id.Value()
                );
            }

            // @TODO Way to trigger on removed.
        }

        if (!(script_component.flags & SCRIPT_COMPONENT_FLAG_VALID)) {
            // Script is not valid, skip
            continue;
        }

        if (!script_component.script_methods[SCRIPT_METHOD_ON_TICK].IsNull()) {
            script->CallFunction(
                script_component.script_methods[SCRIPT_METHOD_ON_TICK],
                script_component.target_object,
                delta
            );
        }
    }
}

} // namespace hyperion::v2