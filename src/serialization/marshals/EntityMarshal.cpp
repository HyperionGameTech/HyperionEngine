/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMArray.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/threading/Threads.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypData.hpp>

#include <core/utilities/Format.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <scene/Entity.hpp>
#include <scene/World.hpp>

#include <scene/EntityManager.hpp>
#include <scene/ComponentInterface.hpp>

// temp
#include <scene/components/MeshComponent.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion::serialization {

template <>
class FBOMMarshaler<Entity> : public HypClassInstanceMarshal
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        FBOMMarshalerBase* nodeMarshal = FBOM::GetInstance().GetMarshal<Node>();
        Assert(nodeMarshal != nullptr);

        if (FBOMResult err = nodeMarshal->Serialize(in, out))
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
                    HYP_LOG(Serialization, Warning, "Entity has multiple components of the type {}", componentInterface->GetTypeName());

                    continue;
                }

                if (componentInterface->IsEntityTag())
                {
                    EntityTag entityTag = componentInterface->GetEntityTag();

                    FBOMObject entityTagObject { FBOMObjectType(componentInterface->GetTypeName(), componentInterface->GetTypeId(), FBOMTypeFlags::DEFAULT) };
                    entityTagObject.SetProperty("EntityTag", uint32(entityTag));
                    out.AddChild(std::move(entityTagObject));

                    serializedComponents.Insert(componentTypeId);

                    continue;
                }

                HYP_NAMED_SCOPE_FMT("Serializing component '{}'", componentInterface->GetTypeName());

                FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(componentTypeId);

                if (!marshal)
                {
                    HYP_LOG(Serialization, Warning, "Cannot serialize component with type name {} and TypeId {} - No marshal registered", componentInterface->GetTypeName(), componentTypeId.Value());

                    continue;
                }

                ConstAnyRef component = entityManager->TryGetComponent(componentTypeId, &entity);
                Assert(component.HasValue());

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
        FBOMMarshalerBase* nodeMarshal = FBOM::GetInstance().GetMarshal<Node>();
        Assert(nodeMarshal != nullptr);

        const HypClass* hypClass = in.GetHypClass();
        Assert(hypClass);

        if (!hypClass->IsDerivedFrom(Entity::Class()))
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object, serialized data with type '{}' (HypClass: {}, TypeId: {}) is not a subclass of Entity", in.GetType().name, hypClass->GetName(), in.GetType().GetNativeTypeId().Value()) };
        }

        if (!hypClass->CreateInstance(out))
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object, HypClass '{}' instance creation failed", hypClass->GetName()) };
        }

        const Handle<Entity>& entity = out.Get<Handle<Entity>>();
        Assert(entity != nullptr);

        if (FBOMResult err = nodeMarshal->Deserialize(context, in, out))
        {
            return err;
        }

        HYP_LOG(Serialization, Debug, "Deserializing Entity of type {} with Id: {}",
            entity->InstanceClass()->GetName(),
            entity->Id());
        
        InitObject(entity);

        // Read components

        for (const FBOMObject& child : in.GetChildren())
        {
            const TypeId childTypeId = child.GetType().GetNativeTypeId();

            if (!childTypeId)
            {
                continue;
            }

            if (!entity->GetEntityManager()->IsValidComponentType(childTypeId))
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
                HYP_LOG(Serialization, Warning, "Component with TypeId {} is not marked for serialization", componentInterface->GetTypeId().Value());

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

                HYP_LOG(Serialization, Debug, "Deserializing entity tag component with value {}", entityTagValue);

                EntityTag entityTag = EntityTag(entityTagValue);

                if (!entity->GetEntityManager()->IsEntityTagComponent(componentInterface->GetTypeId()))
                {
                    HYP_LOG(Serialization, Warning, "Component with TypeId {} is not an entity tag component", componentInterface->GetTypeId().Value());

                    continue;
                }

                // Hack: if the entity tag is static, remove the dynamic tag if it exists and vice versa for dynamic
                switch (entityTag)
                {
                case EntityTag::STATIC:
                    entity->RemoveTag<EntityTag::DYNAMIC>();
                    break;
                case EntityTag::DYNAMIC:
                    entity->RemoveTag<EntityTag::STATIC>();
                    break;
                default:
                    break;
                }

                entity->GetEntityManager()->AddTag(entity, entityTag);

                continue;
            }

            HYP_NAMED_SCOPE_FMT("Deserializing component '{}'", componentInterface->GetTypeName());

            if (!child.m_deserializedObject)
            {
                return { FBOMResult::FBOM_ERR, HYP_FORMAT("No deserialized object found for component '{}'", componentInterface->GetTypeName()) };
            }

            if (entity->GetEntityManager()->HasComponent(childTypeId, entity))
            {
                HYP_LOG(Serialization, Warning, "Entity already has component '{}'", componentInterface->GetTypeName());

                continue;
            }

            HYP_LOG(Serialization, Debug, "Adding component '{}' (child type id: {}, name: {}) to entity of type {} with Id: {}",
                componentInterface->GetTypeName(),
                childTypeId.Value(),
                child.GetType().name,
                entity->InstanceClass()->GetName(),
                entity->Id());

            // temp
            if (componentInterface->GetTypeName() == "MeshComponent")
            {
                HYP_LOG(Serialization, Debug, "MeshComponent deserialized for entity with Id: {}", entity->Id());
                MeshComponent& meshComponent = child.m_deserializedObject->Get<MeshComponent>();
                Assert(meshComponent.mesh.IsValid());
            }

            entity->GetEntityManager()->AddComponent(entity, *child.m_deserializedObject);
        }

        out = HypData(entity);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Entity, FBOMMarshaler<Entity>);

// template <>
// class FBOMMarshaler<ObjId<Entity>> : public FBOMObjectMarshalerBase<ObjId<Entity>>
// {
// public:
//     virtual ~FBOMMarshaler() override = default;

//     virtual FBOMResult Serialize(const ObjId<Entity> &entityId, FBOMObject &out) const override
//     {
//         return FBOMMarshaler<Handle<Entity>>{}.Serialize(Handle<Entity>(entityId), out);
//     }

//     virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
//     {
//         return FBOMMarshaler<Handle<Entity>>{}.Deserialize(in, out);
//     }
// };

// HYP_DEFINE_MARSHAL(ObjId<Entity>, FBOMMarshaler<ObjId<Entity>>);

} // namespace hyperion::serialization
