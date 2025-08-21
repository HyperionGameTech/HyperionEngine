/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <console/commands/LogEntitiesCommand.hpp>

#include <core/object/HypClass.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>

#include <core/json/JSON.hpp>

#include <scene/Scene.hpp>
#include <scene/Node.hpp>
#include <scene/World.hpp>

#include <scene/EntityManager.hpp>
#include <scene/ComponentInterface.hpp>
#include <scene/components/NodeLinkComponent.hpp>
#include <scene/components/UIComponent.hpp>

#include <ui/UIObject.hpp>

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

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

    Handle<World> currentWorld = g_engineDriver->GetCurrentWorld();
    if (!currentWorld)
    {
        return HYP_MAKE_ERROR(Error, "No current world; cannot run command");
    }

    for (const Handle<Scene>& scene : currentWorld->GetScenes())
    {
        AssertDebug(scene != nullptr);

        const Handle<EntityManager>& entityManager = scene->GetEntityManager();
        AssertDebug(entityManager != nullptr);

        json::JSONObject entityManagerJson;

        entityManagerJson["scene"] = scene->GetName().LookupString();
        entityManagerJson["ownerThreadId"] = entityManager->GetOwnerThreadId().GetName().LookupString();

        json::JSONArray entityManagerEntitiesJson;

        // HYP_LOG(LogEntities, Info, "Logging entities for scene {}", entityManager->GetScene()->GetName());

        auto impl = [&]()
        {
            entityManager->ForEachEntity([&](Entity* entity)
                {
                    Assert(entity != nullptr);

                    if (onlyOrphanNodes)
                    {
                        if (entity->GetParent() != nullptr)
                        {
                            // skip
                            return IterationResult::CONTINUE;
                        }
                    }

                    json::JSONObject entityJson;
                    entityJson["id"] = json::JSONString(HYP_FORMAT("{}", entity->Id()));
                    entityJson["refCountStrong"] = entity->GetObjectHeader_Internal()->GetRefCountStrong();
                    entityJson["refCountWeak"] = entity->GetObjectHeader_Internal()->GetRefCountWeak();
                    entityJson["uuid"] = json::JSONString(entity->GetUUID().ToString());
                    entityJson["name"] = json::JSONString(*entity->GetName());
                    entityJson["type"] = json::JSONString(*entity->InstanceClass()->GetName());
                    entityJson["parentName"] = entity->GetParent() ? json::JSONValue(json::JSONString(*entity->GetParent()->GetName())) : json::JSONValue(json::JSONNull());
                    entityJson["parentId"] = entity->GetParent() ? json::JSONValue(json::JSONString(HYP_FORMAT("{}", entity->GetScene()->Id()))) : json::JSONValue(json::JSONNull());
                    entityJson["sceneId"] = entity->GetScene() ? json::JSONValue(json::JSONString(HYP_FORMAT("{}", entity->GetScene()->Id()))) : json::JSONValue(json::JSONNull());
                    entityJson["sceneName"] = entity->GetScene() ? json::JSONValue(json::JSONString(*entity->GetScene()->GetName())) : json::JSONValue(json::JSONNull());

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

                        if (componentTypeId == TypeId::ForType<UIComponent>())
                        {
                            const UIComponent* uiComponent = entityManager->TryGetComponent<UIComponent>(entity);
                            if (uiComponent)
                            {
                                if (Handle<UIObject> uiObject = uiComponent->uiObject.Lock())
                                {
                                    componentJson["ui_object"] = json::JSONObject({ { "name", json::JSONString(*uiObject->GetName()) },
                                        { "type", json::JSONString(*uiObject->InstanceClass()->GetName()) },
                                        { "refCountStrong", uiObject->GetObjectHeader_Internal()->GetRefCountStrong() - 1 },
                                        { "refCountWeak", uiObject->GetObjectHeader_Internal()->GetRefCountWeak() } });
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
    }

    json["entityManagers"] = std::move(entityManagersJson);

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
