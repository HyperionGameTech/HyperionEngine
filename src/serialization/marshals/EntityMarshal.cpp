/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMArray.hpp>
#include <core/serialization/fbom/marshals/ObjectMarshal.hpp>

#include <core/threading/Threads.hpp>

#include <core/reflection/Class.hpp>
#include <core/reflection/Property.hpp>
#include <core/reflection/HypData.hpp>

#include <core/utilities/Format.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <scene/Entity.hpp>
#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <scene/DetachedScene.hpp>

#include <scene/EntityManager.hpp>
#include <scene/ComponentInterface.hpp>

// temp
#include <scene/components/MeshComponent.hpp>
#include <rendering/Mesh.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion::serialization {

class EntityMarshal : public ObjectMarshal
{
public:
    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        if (FBOMResult err = ObjectMarshal::Serialize(in, out))
        {
            return err;
        }

        const Entity& entity = in.Get<Entity>();

        EntityManager* entityManager = entity.GetEntityManager();

        if (!entityManager)
        {
            return { FBOMResult::FBOM_ERR, "Entity not attached to an EntityManager" };
        }

        FBOMResult result = FBOMResult::FBOM_OK;

        auto serializeEntityAndComponents = [&]()
        {
            Optional<const TypeMap<ComponentId>&> allComponents = entityManager->GetAllComponents(&entity);

            if (!allComponents.HasValue())
            {
                result = { FBOMResult::FBOM_ERR, "No component map found for entity" };

                return;
            }

            HashSet<TypeId> serializedComponents;

            for (const auto& it : *allComponents)
            {
                const TypeId componentTypeId = it.first;

                const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);

                if (!componentInterface)
                {
                    result = { FBOMResult::FBOM_ERR, HYP_FORMAT("No ComponentInterface registered for component with TypeId {}", componentTypeId.Value()) };

                    return;
                }

                if (!componentInterface->GetShouldSerialize())
                {
                    continue;
                }

                if (serializedComponents.Contains(componentTypeId))
                {
                    HYP_LOG(Serialization, Warning, "Entity has multiple components of the type {}", componentInterface->GetTypeInfo().name);

                    continue;
                }

                if (componentInterface->IsEntityTag())
                {
                    EntityTag entityTag = componentInterface->GetEntityTag();

                    FBOMObject entityTagObject { FBOMObjectType(*componentInterface->GetTypeInfo().name, componentInterface->GetTypeInfo().id, FBOMTypeFlags::DEFAULT) };
                    entityTagObject.SetProperty("EntityTag", uint32(entityTag));
                    out.AddChild(std::move(entityTagObject));

                    serializedComponents.Insert(componentTypeId);

                    continue;
                }

                HYP_NAMED_SCOPE_FMT("Serializing component '{}'", componentInterface->GetTypeInfo().name);

                FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(componentTypeId);

                if (!marshal)
                {
                    HYP_LOG(Serialization, Warning, "Cannot serialize component of type {} - No marshal registered", componentInterface->GetTypeInfo().name);

                    continue;
                }

                ConstAnyRef component = entityManager->TryGetComponent(componentTypeId, &entity);
                Assert(component.HasValue());

                // temp
                if (componentTypeId == TypeId::ForType<MeshComponent>())
                {
                    const MeshComponent& meshComponent = component.Get<MeshComponent>();
                    if (!meshComponent.mesh || !meshComponent.material)
                    {
                        HYP_BREAKPOINT;
                    }
                }

                FBOMObject componentSerialized;

                if (FBOMResult err = marshal->Serialize(component, componentSerialized))
                {
                    result = err;

                    return;
                }

                out.AddChild(std::move(componentSerialized));

                serializedComponents.Insert(componentTypeId);
            }
        };

        if (Threads::IsOnThread(entityManager->GetOwnerThreadId()))
        {
            serializeEntityAndComponents();
        }
        else
        {
            HYP_NAMED_SCOPE("Awaiting async entity and component serialization");

            Task<void> serializeEntityAndComponentsTask = Threads::GetThread(entityManager->GetOwnerThreadId())->GetScheduler().Enqueue(HYP_STATIC_MESSAGE("Serialize Entity and Components"), [&serializeEntityAndComponents]()
                {
                    serializeEntityAndComponents();
                });

            serializeEntityAndComponentsTask.Await();
        }

        return result;
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        const Class* cls = in.GetClass();
        Assert(cls);

        if (!cls->IsDerivedFrom(Entity::StaticClass()))
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object with ObjectMarshal, serialized data with type '{}' (Class: {}, TypeId: {}) is not a subclass of Entity", in.GetType().name, cls->GetName(), in.GetType().GetNativeTypeId().Value()) };
        }

        if (!cls->CreateInstance(out))
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object with ObjectMarshal, Class '{}' instance creation failed", cls->GetName()) };
        }

        const Handle<Entity>& entity = out.Get<Handle<Entity>>();
        HypData entityData = HypData(entity);

        if (FBOMResult err = ObjectMarshal::Deserialize_Internal(context, in, in.GetClass(), entityData))
        {
            return err;
        }

        HYP_LOG(Serialization, Debug, "Deserializing Entity of type {} with Id: {}",
            entity->InstanceClass()->GetName(),
            entity->Id());

        // Read components

        Scene* detachedScene = GetDetachedSceneForCurrentThread();

        const Handle<EntityManager>& entityManager = detachedScene->GetEntityManager();
        entityManager->AddExistingEntity(entity);

        for (const FBOMObject& child : in.GetChildren())
        {
            const TypeId childTypeId = child.GetType().GetNativeTypeId();

            if (!childTypeId)
            {
                continue;
            }

            if (!entityManager->IsValidComponentType(childTypeId))
            {
                HYP_LOG(Serialization, Warning, "Component with TypeId {} is not a valid component type", childTypeId.Value());

                continue;
            }

            const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(childTypeId);

            if (!componentInterface)
            {
                HYP_LOG(Serialization, Warning, "No ComponentInterface registered for component with TypeId {} (serialized object type name: {})", childTypeId.Value(), child.GetType().name);

                continue;
            }

            if (!componentInterface->GetShouldSerialize())
            {
                HYP_LOG(Serialization, Warning, "Component with TypeId {} is not marked for serialization", componentInterface->GetTypeInfo().name);

                continue;
            }

            if (componentInterface->IsEntityTag())
            {
                HYP_NAMED_SCOPE("Deserializing entity tag");

                uint32 entityTagValue = 0;

                if (FBOMResult err = child.GetProperty("EntityTag").ReadUInt32(&entityTagValue))
                {
                    return err;
                }

                EntityTag entityTag = EntityTag(entityTagValue);

                if (!entityManager->IsEntityTagComponent(componentInterface->GetTypeInfo().id))
                {
                    HYP_LOG(Serialization, Warning, "Component with TypeId {} is not an entity tag component", componentInterface->GetTypeInfo().name);

                    continue;
                }

                // Hack: if the entity tag is static, remove the dynamic tag if it exists and vice versa for dynamic
                switch (entityTag)
                {
                case EntityTag::STATIC:
                    entityManager->RemoveTag<EntityTag::DYNAMIC>(entity);
                    break;
                case EntityTag::DYNAMIC:
                    entityManager->RemoveTag<EntityTag::STATIC>(entity);
                    break;
                default:
                    break;
                }

                entityManager->AddTag(entity, entityTag);

                continue;
            }

            HYP_NAMED_SCOPE_FMT("Deserializing component '{}'", componentInterface->GetTypeInfo().name);

            if (!child.m_deserializedObject)
            {
                return { FBOMResult::FBOM_ERR, HYP_FORMAT("No deserialized object found for component '{}'", componentInterface->GetTypeInfo().name) };
            }

            if (entityManager->HasComponent(childTypeId, entity))
            {
                HYP_LOG(Serialization, Warning, "Entity already has component '{}'", componentInterface->GetTypeInfo().name);

                continue;
            }

            HYP_LOG(Serialization, Debug, "Adding component '{}' (child type id: {}, name: {}) to entity of type {} with Id: {}",
                componentInterface->GetTypeInfo().name,
                childTypeId.Value(),
                child.GetType().name,
                entity->InstanceClass()->GetName(),
                entity->Id());

            entityManager->AddComponent(entity, *child.m_deserializedObject);
        }

        out = std::move(entityData);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Entity, EntityMarshal);

} // namespace hyperion::serialization
