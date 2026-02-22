/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/console/commands/LogEntitiesCommand.hpp>

#include <Core/io/ByteWriter.hpp>

#include <Core/threading/Threads.hpp>

#include <Core/json/JSON.hpp>

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

    JSON::Object json;

    JSON::JArray entityManagersJson;

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

        JSON::Object entityManagerJson;

        entityManagerJson["scene"] = scene->GetName().LookupString();
        entityManagerJson["ownerThreadId"] = entityManager->GetOwnerThreadId().GetName().LookupString();

        JSON::JArray entityManagerEntitiesJson;

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

                    JSON::Object entityJson;
                    entityJson["id"] = JSON::JString(HYP_FORMAT("{}", entity->Id()));
                    entityJson["refCountStrong"] = entity->GetObjectHeader_Internal()->GetRefCountStrong();
                    entityJson["refCountWeak"] = entity->GetObjectHeader_Internal()->GetRefCountWeak();
                    entityJson["name"] = JSON::JString(*entity->GetName());
                    entityJson["type"] = JSON::JString(*entity->InstanceClass()->GetName());
                    entityJson["parentName"] = entity->GetParent() ? JSON::Value(JSON::JString(*entity->GetParent()->GetName())) : JSON::Value(JSON::JSNull());
                    entityJson["parentId"] = entity->GetParent() ? JSON::Value(JSON::JString(HYP_FORMAT("{}", entity->GetScene()->Id()))) : JSON::Value(JSON::JSNull());
                    entityJson["sceneId"] = entity->GetScene() ? JSON::Value(JSON::JString(HYP_FORMAT("{}", entity->GetScene()->Id()))) : JSON::Value(JSON::JSNull());
                    entityJson["sceneName"] = entity->GetScene() ? JSON::Value(JSON::JString(*entity->GetScene()->GetName())) : JSON::Value(JSON::JSNull());

                    JSON::JArray componentsJson;

                    for (const auto& it : *entityManager->GetAllComponents(entity))
                    {
                        const TypeId componentTypeId = it.first;
                        const ComponentId componentId = it.second;

                        const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);

                        if (!componentInterface)
                        {
                            continue;
                        }

                        JSON::Object componentJson;
                        componentJson["type"] = *componentInterface->GetTypeInfo().name;
                        componentJson["id"] = uint32(componentId);

                        if (componentTypeId == TypeId::ForType<UIComponent>())
                        {
                            const UIComponent* uiComponent = entityManager->TryGetComponent<UIComponent>(entity);
                            if (uiComponent)
                            {
                                if (Handle<UIObject> uiObject = uiComponent->uiObject.Lock())
                                {
                                    componentJson["ui_object"] = JSON::Object({ { "name", JSON::JString(*uiObject->GetName()) },
                                        { "type", JSON::JString(*uiObject->InstanceClass()->GetName()) },
                                        { "refCountStrong", uiObject->GetObjectHeader_Internal()->GetRefCountStrong() - 1 },
                                        { "refCountWeak", uiObject->GetObjectHeader_Internal()->GetRefCountWeak() }
                                    });
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
    writer.WriteString(JSON::Value(json).ToString(true).ToUtf8());
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
