/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <console/commands/LogEntitiesCommand.hpp>

#include <core/object/HypClass.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>

#include <core/json/JSON.hpp>

#include <scene/Scene.hpp>
#include <scene/Node.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/ComponentInterface.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>
#include <scene/ecs/components/UIComponent.hpp>

#include <ui/UIObject.hpp>

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Console);
HYP_DEFINE_LOG_SUBCHANNEL(LogEntities, Console);

Result LogEntitiesCommand::Execute_Impl(const CommandLineArguments &args)
{
    HYP_LOG(LogEntities, Info, "LogEntitiesCommand test");

    // Trigger .NET GC and wait for finalizers (there may be entities waiting to be GC'd)
    dotnet::DotNetSystem::GetInstance().GetGlobalFunctions().trigger_gc_function();
    Threads::Sleep(1000);

    String file_arg = args["file"].ToString();

    if (!file_arg.EndsWith(".json")) {
        file_arg += ".json";
    }

    const bool only_orphan_nodes = args["orphans"].ToBool(false);

    json::JSONObject json;

    json::JSONArray entity_managers_json;

    EntityManager::GetEntityToEntityManagerMap().ForEachEntityManager([&](EntityManager *entity_manager)
    {
        json::JSONObject entity_manager_json;

        if (!entity_manager->GetScene()) {
            return;
        }

        entity_manager_json["scene"] = entity_manager->GetScene()->GetName().LookupString();
        entity_manager_json["owner_thread_id"] = entity_manager->GetOwnerThreadID().GetName().LookupString();

        json::JSONArray entity_manager_entities_json;
        
        // HYP_LOG(LogEntities, Info, "Logging entities for scene {}", entity_manager->GetScene()->GetName());

        entity_manager->ForEachEntity([&](const Handle<Entity> &entity, const EntityData &entity_data)
        {
            if (only_orphan_nodes) {
                NodeLinkComponent *node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(entity);

                if (!node_link_component) {
                    return IterationResult::CONTINUE;
                }

                RC<Node> node = node_link_component->node.Lock();
                
                if (node == nullptr || node->GetParent() != nullptr) {
                    return IterationResult::CONTINUE;
                }

                if (entity_manager->GetScene()->GetRoot().Get() == node.Get()) {
                    return IterationResult::CONTINUE; // skip root node
                }
            }

            json::JSONObject entity_json;
            entity_json["id"] = entity.GetID().Value();

            entity_json["num_strong_refs"] = entity.ptr->GetRefCountStrong() - 1;
            entity_json["num_weak_refs"] = entity.ptr->GetRefCountWeak();
            
            json::JSONArray components_json;

            for (const auto &it : *entity_manager->GetAllComponents(entity)) {
                const TypeID component_type_id = it.first;
                const ComponentID component_id = it.second;

                const IComponentInterface *component_interface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(component_type_id);

                if (!component_interface) {
                    continue;
                }

                json::JSONObject component_json;
                component_json["type"] = component_interface->GetTypeName();
                component_json["id"] = component_id;

                if (component_type_id == TypeID::ForType<NodeLinkComponent>()) {
                    const NodeLinkComponent *node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(entity);
                    if (node_link_component) {
                        if (RC<Node> node = node_link_component->node.Lock()) {
                            component_json["node"] = json::JSONObject({
                                { "uuid", json::JSONString(node->GetUUID().ToString()) },
                                { "name", json::JSONString(*node->GetName()) },
                                { "type", json::JSONString(*node->InstanceClass()->GetName()) },
                                { "num_strong_refs", node.GetRefCountData_Internal()->UseCount_Strong() - 1 },
                                { "num_weak_refs", node.GetRefCountData_Internal()->UseCount_Weak() },
                                { "parent_name", node->GetParent() ? json::JSONValue(json::JSONString(*node->GetParent()->GetName())) : json::JSONValue(json::JSONNull()) },
                                { "parent_uuid", node->GetParent() ? json::JSONValue(json::JSONString(node->GetParent()->GetUUID().ToString())) : json::JSONValue(json::JSONNull()) },
                                { "scene_id", node->GetScene() ? json::JSONValue(node->GetScene()->GetID().Value()) : json::JSONValue(uint32(0)) },
                                { "scene_name", node->GetScene() ? json::JSONValue(json::JSONString(*node->GetScene()->GetName())) : json::JSONValue(json::JSONNull()) }
                            });
                            
                        } else {
                            component_json["node"] = "<expired>";
                        }
                    }
                } else if (component_type_id == TypeID::ForType<UIComponent>()) {
                    const UIComponent *ui_component = entity_manager->TryGetComponent<UIComponent>(entity);
                    if (ui_component) {
                        if (UIObject *ui_object = ui_component->ui_object) {
                            RC<UIObject> ui_object_ref = ui_object->RefCountedPtrFromThis();
                            AssertThrow(ui_object_ref.IsValid());

                            component_json["ui_object"] = json::JSONObject({
                                { "name", json::JSONString(*ui_object->GetName()) },
                                { "type", json::JSONString(*ui_object->InstanceClass()->GetName()) },
                                { "num_strong_refs", ui_object_ref.GetRefCountData_Internal()->UseCount_Strong() - 1 },
                                { "num_weak_refs", ui_object_ref.GetRefCountData_Internal()->UseCount_Weak() }
                            });
                        }
                    }
                }

                components_json.PushBack(std::move(component_json));
            }

            entity_json["components"] = std::move(components_json);
            entity_manager_entities_json.PushBack(std::move(entity_json));

            return IterationResult::CONTINUE;
        });

        entity_manager_json["entities"] = std::move(entity_manager_entities_json);

        entity_managers_json.PushBack(std::move(entity_manager_json));
    });

    json["entity_managers"] = std::move(entity_managers_json);

    FilePath filepath = FilePath::Current() / file_arg;
    if (!filepath.BasePath().MkDir()) {
        return HYP_MAKE_ERROR(Error, "Failed to create directory for file {}", filepath.BasePath());
    }

    FileByteWriter writer(filepath.Data());
    writer.WriteString(json::JSONValue(json).ToString(true));
    writer.Close();

    return { };
}

CommandLineArgumentDefinitions LogEntitiesCommand::GetDefinitions_Internal() const
{
    CommandLineArgumentDefinitions definitions;
    definitions.Add("file", "f", "The file to log to", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING, "entities.json");
    definitions.Add("orphans", "", "Include only orphan nodes (not attached to root)", CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);

    return definitions;
}

} // namespace hyperion
