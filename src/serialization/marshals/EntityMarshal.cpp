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

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/ComponentInterface.hpp>

// temp
#include <scene/ecs/components/MeshComponent.hpp>

#include <Engine.hpp>

namespace hyperion::serialization {

template <>
class FBOMMarshaler<Entity> : public HypClassInstanceMarshal
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out))
        {
            return err;
        }

        const Entity& entity = in.Get<Entity>();

        EntityManager* entity_manager = entity.GetEntityManager();

        if (!entity_manager)
        {
            return { FBOMResult::FBOM_ERR, "Entity not attached to an EntityManager" };
        }

        FBOMResult result = FBOMResult::FBOM_OK;

        auto serialize_entity_and_components = [&]()
        {
            Optional<const TypeMap<ComponentID>&> all_components = entity_manager->GetAllComponents(&entity);

            if (!all_components.HasValue())
            {
                result = { FBOMResult::FBOM_ERR, "No component map found for entity" };

                return;
            }

            HashSet<TypeId> serialized_components;

            for (const auto& it : *all_components)
            {
                const TypeId component_type_id = it.first;

                const IComponentInterface* component_interface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(component_type_id);

                if (!component_interface)
                {
                    result = { FBOMResult::FBOM_ERR, HYP_FORMAT("No ComponentInterface registered for component with TypeId {}", component_type_id.Value()) };

                    return;
                }

                if (!component_interface->GetShouldSerialize())
                {
                    continue;
                }

                if (serialized_components.Contains(component_type_id))
                {
                    HYP_LOG(Serialization, Warning, "Entity has multiple components of the type {}", component_interface->GetTypeName());

                    continue;
                }

                if (component_interface->IsEntityTag())
                {
                    EntityTag entity_tag = component_interface->GetEntityTag();

                    FBOMObject entity_tag_object { FBOMObjectType(component_interface->GetTypeName(), component_interface->GetTypeId(), FBOMTypeFlags::DEFAULT) };
                    entity_tag_object.SetProperty("EntityTag", uint32(entity_tag));
                    out.AddChild(std::move(entity_tag_object));

                    serialized_components.Insert(component_type_id);

                    continue;
                }

                HYP_NAMED_SCOPE_FMT("Serializing component '{}'", component_interface->GetTypeName());

                FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(component_type_id);

                if (!marshal)
                {
                    HYP_LOG(Serialization, Warning, "Cannot serialize component with type name {} and TypeId {} - No marshal registered", component_interface->GetTypeName(), component_type_id.Value());

                    continue;
                }

                ConstAnyRef component = entity_manager->TryGetComponent(component_type_id, &entity);
                AssertThrow(component.HasValue());

                FBOMObject component_serialized;

                if (FBOMResult err = marshal->Serialize(component, component_serialized))
                {
                    result = err;

                    return;
                }

                out.AddChild(std::move(component_serialized));

                serialized_components.Insert(component_type_id);
            }
        };

        if (Threads::IsOnThread(entity_manager->GetOwnerThreadId()))
        {
            serialize_entity_and_components();
        }
        else
        {
            HYP_NAMED_SCOPE("Awaiting async entity and component serialization");

            Task<void> serialize_entity_and_components_task = Threads::GetThread(entity_manager->GetOwnerThreadId())->GetScheduler().Enqueue(HYP_STATIC_MESSAGE("Serialize Entity and Components"), [&serialize_entity_and_components]()
                {
                    serialize_entity_and_components();
                });

            serialize_entity_and_components_task.Await();
        }

        return result;
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        const HypClass* hyp_class = in.GetHypClass();
        AssertThrow(hyp_class);

        if (!hyp_class->IsDerivedFrom(Entity::Class()))
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object with HypClassInstanceMarshal, serialized data with type '{}' (HypClass: {}, TypeId: {}) is not a subclass of Entity", in.GetType().name, hyp_class->GetName(), in.GetType().GetNativeTypeId().Value()) };
        }

        if (!hyp_class->CreateInstance(out))
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object with HypClassInstanceMarshal, HypClass '{}' instance creation failed", hyp_class->GetName()) };
        }

        const Handle<Entity>& entity = out.Get<Handle<Entity>>();

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(context, in, in.GetHypClass(), entity.ToRef()))
        {
            return err;
        }

        HYP_LOG(Serialization, Debug, "Deserializing Entity of type {} with Id: {}",
            entity->InstanceClass()->GetName(),
            entity->GetID());

        // Read components

        const Handle<Scene>& detached_scene = g_engine->GetDefaultWorld()->GetDetachedScene(ThreadId::Current());
        const Handle<EntityManager>& entity_manager = detached_scene->GetEntityManager();
        entity_manager->AddExistingEntity(entity);

        for (const FBOMObject& child : in.GetChildren())
        {
            const TypeId child_type_id = child.GetType().GetNativeTypeId();

            if (!child_type_id)
            {
                continue;
            }

            if (!entity_manager->IsValidComponentType(child_type_id))
            {
                HYP_LOG(Serialization, Warning, "Component with TypeId {} is not a valid component type", child_type_id.Value());

                continue;
            }

            const IComponentInterface* component_interface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(child_type_id);

            if (!component_interface)
            {
                HYP_LOG(Serialization, Warning, "No ComponentInterface registered for component with TypeId {} (serialized object type name: {})", child_type_id.Value(), child.GetType().name);

                continue;
            }

            if (!component_interface->GetShouldSerialize())
            {
                HYP_LOG(Serialization, Warning, "Component with TypeId {} is not marked for serialization", component_interface->GetTypeId().Value());

                continue;
            }

            if (component_interface->IsEntityTag())
            {
                HYP_NAMED_SCOPE("Deserializing entity tag");

                uint32 entity_tag_value = 0;

                if (FBOMResult err = child.GetProperty("EntityTag").ReadUInt32(&entity_tag_value))
                {
                    return err;
                }

                HYP_LOG(Serialization, Debug, "Deserializing entity tag component with value {}", entity_tag_value);

                EntityTag entity_tag = EntityTag(entity_tag_value);

                if (!entity_manager->IsEntityTagComponent(component_interface->GetTypeId()))
                {
                    HYP_LOG(Serialization, Warning, "Component with TypeId {} is not an entity tag component", component_interface->GetTypeId().Value());

                    continue;
                }

                // Hack: if the entity tag is static, remove the dynamic tag if it exists and vice versa for dynamic
                switch (entity_tag)
                {
                case EntityTag::STATIC:
                    entity_manager->RemoveTag<EntityTag::DYNAMIC>(entity);
                    break;
                case EntityTag::DYNAMIC:
                    entity_manager->RemoveTag<EntityTag::STATIC>(entity);
                    break;
                default:
                    break;
                }

                entity_manager->AddTag(entity, entity_tag);

                continue;
            }

            HYP_NAMED_SCOPE_FMT("Deserializing component '{}'", component_interface->GetTypeName());

            if (!child.m_deserialized_object)
            {
                return { FBOMResult::FBOM_ERR, HYP_FORMAT("No deserialized object found for component '{}'", component_interface->GetTypeName()) };
            }

            if (entity_manager->HasComponent(child_type_id, entity))
            {
                HYP_LOG(Serialization, Warning, "Entity already has component '{}'", component_interface->GetTypeName());

                continue;
            }

            HYP_LOG(Serialization, Debug, "Adding component '{}' (child type id: {}, name: {}) to entity of type {} with Id: {}",
                component_interface->GetTypeName(),
                child_type_id.Value(),
                child.GetType().name,
                entity->InstanceClass()->GetName(),
                entity->GetID());

            // temp
            if (component_interface->GetTypeName() == "MeshComponent")
            {
                HYP_LOG(Serialization, Debug, "MeshComponent deserialized for entity with Id: {}", entity->GetID());
                MeshComponent& mesh_component = child.m_deserialized_object->Get<MeshComponent>();
                AssertThrow(mesh_component.mesh.IsValid());
            }

            entity_manager->AddComponent(entity, *child.m_deserialized_object);
        }

        out = HypData(entity);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Entity, FBOMMarshaler<Entity>);

// template <>
// class FBOMMarshaler<Id<Entity>> : public FBOMObjectMarshalerBase<Id<Entity>>
// {
// public:
//     virtual ~FBOMMarshaler() override = default;

//     virtual FBOMResult Serialize(const Id<Entity> &entity_id, FBOMObject &out) const override
//     {
//         return FBOMMarshaler<Handle<Entity>>{}.Serialize(Handle<Entity>(entity_id), out);
//     }

//     virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
//     {
//         return FBOMMarshaler<Handle<Entity>>{}.Deserialize(in, out);
//     }
// };

// HYP_DEFINE_MARSHAL(Id<Entity>, FBOMMarshaler<Id<Entity>>);

} // namespace hyperion::serialization