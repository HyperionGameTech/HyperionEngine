/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/console/commands/LogEntitiesCommand.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/threading/Threads.hpp>

#include <core/json/JSON.hpp>

#include <scene/Scene.hpp>
#include <scene/Node.hpp>
#include <scene/World.hpp>
#include <scene/EntityManager.hpp>
#include <scene/ComponentInterface.hpp>
#include <scene/components/UIComponent.hpp>

#include <ui/UIObject.hpp>

#include <dotnet/DotNETHost.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <LogEntitiesCommand.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Console);
HYP_DEFINE_LOG_SUBCHANNEL(LogEntities, Console);

Result LogEntitiesCommand::Execute_Impl(const CommandLineArguments& args)
{
    HYP_LOG(LogEntities, Info, "LogEntitiesCommand test");

    if (DotNETHost::GetInstance().GetGlobalFunctions().triggerGcFunction)
    {
        // Trigger .NET garbage collector and wait for finalizers (there may be entities waiting to be collected)
        DotNETHost::GetInstance().GetGlobalFunctions().triggerGcFunction();
        ThreadSleep(1000);
    }

    String fileArg = args["file"].ToString();

    if (!fileArg.EndsWith(".json"))
    {
        fileArg += ".json";
    }

    const bool onlyOrphanNodes = args["orphans"].ToBool(false);

    Json::JSObject json;

    Json::JSArray entityManagersJson;

    World* currentWorld = g_engineDriver->GetCurrentWorld();
    if (!currentWorld)
    {
        return HYP_MAKE_ERROR(Error, "No current world; cannot run command");
    }

    for (const Handle<Scene>& scene : currentWorld->GetScenes())
    {
        AssertDebug(scene != nullptr);

        const Handle<EntityManager>& entityManager = scene->GetEntityManager();
        AssertDebug(entityManager != nullptr);

        Json::JSObject entityManagerJson;

        entityManagerJson["scene"] = scene->GetName().LookupString();
        entityManagerJson["ownerThreadId"] = entityManager->GetOwnerThreadId().GetName().LookupString();

        Json::JSArray entityManagerEntitiesJson;

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

                    Json::JSObject entityJson;
                    entityJson["id"] = Json::JSString(HYP_FORMAT("{}", entity->Id()));
                    entityJson["refCountStrong"] = entity->GetObjectHeader_Internal()->GetRefCountStrong();
                    entityJson["refCountWeak"] = entity->GetObjectHeader_Internal()->GetRefCountWeak();
                    entityJson["uuid"] = Json::JSString(entity->GetUUID().ToString());
                    entityJson["name"] = Json::JSString(*entity->GetName());
                    entityJson["type"] = Json::JSString(*entity->InstanceClass()->GetName());
                    entityJson["parentName"] = entity->GetParent() ? Json::Value(Json::JSString(*entity->GetParent()->GetName())) : Json::Value(Json::JSNull());
                    entityJson["parentId"] = entity->GetParent() ? Json::Value(Json::JSString(HYP_FORMAT("{}", entity->GetScene()->Id()))) : Json::Value(Json::JSNull());
                    entityJson["sceneId"] = entity->GetScene() ? Json::Value(Json::JSString(HYP_FORMAT("{}", entity->GetScene()->Id()))) : Json::Value(Json::JSNull());
                    entityJson["sceneName"] = entity->GetScene() ? Json::Value(Json::JSString(*entity->GetScene()->GetName())) : Json::Value(Json::JSNull());

                    Json::JSArray componentsJson;

                    for (const auto& it : *entityManager->GetAllComponents(entity))
                    {
                        const TypeId componentTypeId = it.first;
                        const ComponentId componentId = it.second;

                        const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);

                        if (!componentInterface)
                        {
                            continue;
                        }

                        Json::JSObject componentJson;
                        componentJson["type"] = *componentInterface->GetTypeInfo().name;
                        componentJson["id"] = uint32(componentId);

                        if (componentTypeId == TypeId::ForType<UIComponent>())
                        {
                            const UIComponent* uiComponent = entityManager->TryGetComponent<UIComponent>(entity);
                            if (uiComponent)
                            {
                                if (Handle<UIObject> uiObject = uiComponent->uiObject.Lock())
                                {
                                    componentJson["ui_object"] = Json::JSObject({ { "name", Json::JSString(*uiObject->GetName()) },
                                        { "type", Json::JSString(*uiObject->InstanceClass()->GetName()) },
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

        if (CurrentThreadId() == entityManager->GetOwnerThreadId())
        {
            impl();
        }
        else
        {
            Task task = GetThreadById(entityManager->GetOwnerThreadId())->GetScheduler().Enqueue(std::move(impl));
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
    writer.WriteString(Json::Value(json).ToString(true).ToUtf8());
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

} // namespace Hyperion
