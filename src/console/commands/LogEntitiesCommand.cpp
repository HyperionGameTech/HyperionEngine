/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <console/commands/LogEntitiesCommand.hpp>

#include <core/object/HypClass.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>

#include <core/json/JSON.hpp>

#include <scene/Scene.hpp>
#include <scene/Node.hpp>

#include <scene/EntityManager.hpp>
#include <scene/ComponentInterface.hpp>
#include <scene/components/NodeLinkComponent.hpp>
#include <scene/EntityTag.hpp>

#include <ui/UIObject.hpp>

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Console);
HYP_DEFINE_LOG_SUBCHANNEL(LogEntities, Console);

Result LogEntitiesCommand::Execute_Impl(const CommandLineArguments& args)
{
    HYP_LOG(LogEntities, Info, "LogEntitiesCommand test");

    // Trigger .NET GC and wait for finalizers (there may be entities waiting to be GC'd)
    dotnet::DotNetSystem::GetInstance().GetGlobalFunctions().triggerGcFunction();
    Threads::Sleep(1000);

    String fileArg = args["file"].ToString();

    if (!fileArg.EndsWith(".json"))
    {
        fileArg += ".json";
    }

    const bool onlyOrphanNodes = args["orphans"].ToBool(false);

    json::JSONObject json;

    json::JSONArray entityManagersJson;

#if 0 // fixme: No more entity to entitymanager map
    EntityManager::GetEntityToEntityManagerMap().ForEachEntityManager([&](EntityManager* entityManager)
        {
            json::JSONObject entityManagerJson;

            if (!entityManager->GetScene())
            {
                return;
            }

            entityManagerJson["scene"] = entityManager->GetScene()->GetName().LookupString();
            entityManagerJson["owner_thread_id"] = entityManager->GetOwnerThreadId().GetName().LookupString();

            json::JSONArray entityManagerEntitiesJson;

            // HYP_LOG(LogEntities, Info, "Logging entities for scene {}", entityManager->GetScene()->GetName());

            auto impl = [&]()
            {
                entityManager->ForEachEntity([&](const Handle<Entity>& entity, const EntityData& entityData)
                    {
                        if (onlyOrphanNodes)
                        {
                            NodeLinkComponent* nodeLinkComponent = entityManager->TryGetComponent<NodeLinkComponent>(entity);

                            if (!nodeLinkComponent)
                            {
                                return IterationResult::CONTINUE;
                            }

                            Handle<Node> node = nodeLinkComponent->node.Lock();

                            if (node == nullptr || node->GetParent() != nullptr)
                            {
                                return IterationResult::CONTINUE;
                            }

                            if (entityManager->GetScene()->GetRoot().Get() == node.Get())
                            {
                                return IterationResult::CONTINUE; // skip root node
                            }
                        }

                        json::JSONObject entityJson;
                        entityJson["id"] = entity->Id().Value();

                        entityJson["num_strong_refs"] = entity.ptr->GetObjectHeader_Internal()->GetRefCountStrong() - 1;
                        entityJson["num_weak_refs"] = entity.ptr->GetObjectHeader_Internal()->GetRefCountWeak();

                        json::JSONArray componentsJson;

                        for (const auto& it : *entityManager->GetAllComponents(entity))
                        {
                            const TypeId componentTypeId = it.first;
                            const ComponentId componentId = it.second;

                            const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);

                            if (!componentInterface)
                            {
                                continue;
                            }

                            json::JSONObject componentJson;
                            componentJson["type"] = componentInterface->GetTypeName();
                            componentJson["id"] = componentId;

                            if (componentTypeId == TypeId::ForType<NodeLinkComponent>())
                            {
                                const NodeLinkComponent* nodeLinkComponent = entityManager->TryGetComponent<NodeLinkComponent>(entity);
                                if (nodeLinkComponent)
                                {
                                    if (Handle<Node> node = nodeLinkComponent->node.Lock())
                                    {
                                        componentJson["node"] = json::JSONObject({ { "uuid", json::JSONString(node->GetUUID().ToString()) },
                                            { "name", json::JSONString(*node->GetName()) },
                                            { "type", json::JSONString(*node->InstanceClass()->GetName()) },
                                            { "num_strong_refs", node->GetObjectHeader_Internal()->GetRefCountStrong() - 1 },
                                            { "num_weak_refs", node->GetObjectHeader_Internal()->GetRefCountWeak() },
                                            { "parent_name", node->GetParent() ? json::JSONValue(json::JSONString(*node->GetParent()->GetName())) : json::JSONValue(json::JSONNull()) },
                                            { "parent_uuid", node->GetParent() ? json::JSONValue(json::JSONString(node->GetParent()->GetUUID().ToString())) : json::JSONValue(json::JSONNull()) },
                                            { "scene_id", node->GetScene() ? json::JSONValue(node->GetScene()->Id().Value()) : json::JSONValue(uint32(0)) },
                                            { "scene_name", node->GetScene() ? json::JSONValue(json::JSONString(*node->GetScene()->GetName())) : json::JSONValue(json::JSONNull()) } });
                                    }
                                    else
                                    {
                                        componentJson["node"] = "<expired>";
                                    }
                                }
                            }
                            else if (componentTypeId == TypeId::ForType<UIComponent>())
                            {
                                const UIComponent* uiComponent = entityManager->TryGetComponent<UIComponent>(entity);
                                if (uiComponent)
                                {
                                    if (UIObject* uiObject = uiComponent->uiObject)
                                    {
                                        Handle<UIObject> uiObjectRef = uiObject->HandleFromThis();
                                        Assert(uiObjectRef.IsValid());

                                        componentJson["ui_object"] = json::JSONObject({ { "name", json::JSONString(*uiObject->GetName()) },
                                            { "type", json::JSONString(*uiObject->InstanceClass()->GetName()) },
                                            { "num_strong_refs", uiObjectRef->GetObjectHeader_Internal()->GetRefCountStrong() - 1 },
                                            { "num_weak_refs", uiObjectRef->GetObjectHeader_Internal()->GetRefCountWeak() } });
                                    }
                                }
                            }

                            componentsJson.PushBack(std::move(componentJson));
                        }

                        entityJson["components"] = std::move(componentsJson);
                        entityManagerEntitiesJson.PushBack(std::move(entityJson));

                        return IterationResult::CONTINUE;
                    });
            };

            if (Threads::CurrentThreadId() == entityManager->GetOwnerThreadId())
            {
                impl();
            }
            else
            {
                Task task = Threads::GetThread(entityManager->GetOwnerThreadId())->GetScheduler().Enqueue(std::move(impl));
                task.Await();
            }

            entityManagerJson["entities"] = std::move(entityManagerEntitiesJson);

            entityManagersJson.PushBack(std::move(entityManagerJson));
        });
#endif

    json["entity_managers"] = std::move(entityManagersJson);

    FilePath filepath = FilePath::Current() / fileArg;
    if (!filepath.BasePath().MkDir())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create directory for file {}", filepath.BasePath());
    }

    FileByteWriter writer(filepath.Data());
    writer.WriteString(json::JSONValue(json).ToString(true));
    writer.Close();

    return {};
}

CommandLineArgumentDefinitions LogEntitiesCommand::GetDefinitions_Internal() const
{
    CommandLineArgumentDefinitions definitions;
    definitions.Add("file", "f", "The file to log to", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING, "entities.json");
    definitions.Add("orphans", "", "Include only orphan nodes (not attached to root)", CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);

    return definitions;
}

} // namespace hyperion
