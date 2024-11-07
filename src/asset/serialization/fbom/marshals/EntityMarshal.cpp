/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/FBOMArray.hpp>

#include <scene/Entity.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/threading/Threads.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypData.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Entity> : public FBOMObjectMarshalerBase<Entity>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const Entity &entity, FBOMObject &out) const override
    {
        // out.SetProperty("IsValid", entity.IsValid());

        // if (!entity.IsValid()) {
        //     return { FBOMResult::FBOM_OK };
        // }
        
        EntityManager *entity_manager = EntityManager::GetEntityToEntityManagerMap().GetEntityManager(entity.GetID());

        if (!entity_manager) {
            return { FBOMResult::FBOM_ERR, "Entity not attached to an EntityManager" };
        }

        FBOMResult result = FBOMResult::FBOM_OK;

        auto SerializeEntityAndComponents = [&]()
        {
            Optional<const TypeMap<ComponentID> &> all_components = entity_manager->GetAllComponents(entity.GetID());

            if (!all_components.HasValue()) {
                result = { FBOMResult::FBOM_ERR, "No component map found for entity" };

                return;
            }

            FlatSet<TypeID> serialized_components;

            for (const auto &it : *all_components) {
                const TypeID component_type_id = it.first;

                const ComponentInterface *component_interface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(component_type_id);

                if (!component_interface) {
                    result = { FBOMResult::FBOM_ERR, HYP_FORMAT("No ComponentInterface registered for component with TypeID {}", component_type_id.Value()) };

                    return;
                }

                if (component_interface->GetClass() != nullptr && component_interface->GetClass()->GetAttribute("serialize") == false) {
                    HYP_LOG(Serialization, LogLevel::INFO, "HypClass for component '{}' has the Serialize attribute set to false; skipping", component_interface->GetTypeName());

                    continue;
                }

                if (serialized_components.Contains(component_type_id)) {
                    HYP_LOG(Serialization, LogLevel::WARNING, "Entity has multiple components of the type {}", component_interface->GetTypeName());

                    continue;
                }

                HYP_NAMED_SCOPE_FMT("Serializing component '{}'", component_interface->GetTypeName());

                FBOMMarshalerBase *marshal = FBOM::GetInstance().GetMarshal(component_type_id);

                if (!marshal) {
                    HYP_LOG(Serialization, LogLevel::WARNING, "Cannot serialize component with TypeID {} - No marshal registered", component_type_id.Value());

                    continue;
                }

                ConstAnyRef component = entity_manager->TryGetComponent(component_type_id, entity.GetID());
                AssertThrow(component.HasValue());

                FBOMObject component_serialized;

                if (FBOMResult err = marshal->Serialize(component, component_serialized)) {
                    result = err;

                    return;
                }

                out.AddChild(std::move(component_serialized));

                serialized_components.Insert(component_type_id);
            }
        };

        if (entity_manager->GetOwnerThreadMask() & Threads::CurrentThreadID()) {
            SerializeEntityAndComponents();
        } else {
            HYP_NAMED_SCOPE("Awaiting async entity and component serialization");

            entity_manager->PushCommand([&SerializeEntityAndComponents](EntityManager &mgr, GameCounter::TickUnit delta)
            {
                SerializeEntityAndComponents();
            });

            entity_manager->GetCommandQueue().AwaitEmpty();
        }

        return result;
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        // bool is_valid = false;

        // if (FBOMResult err = in.GetProperty("IsValid").ReadBool(&is_valid)) {
        //     return err;
        // }

        // if (!is_valid) {
        //     out = HypData(Handle<Entity>::empty);

        //     return FBOMResult::FBOM_OK;
        // }

        const Handle<Scene> &detached_scene = g_engine->GetDefaultWorld()->GetDetachedScene(ThreadID::Current());
        const RC<EntityManager> &entity_manager = detached_scene->GetEntityManager();

        Handle<Entity> entity = entity_manager->AddEntity();

        for (FBOMObject &subobject : *in.nodes) {
            const TypeID subobject_type_id = subobject.GetType().GetNativeTypeID();

            if (!subobject_type_id) {
                continue;
            }

            if (!entity_manager->IsValidComponentType(subobject_type_id)) {
                continue;
            }
            
            const ComponentInterface *component_interface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(subobject_type_id);

            if (!component_interface) {
                HYP_LOG(Serialization, LogLevel::WARNING, "No ComponentInterface registered for component with TypeID {} (serialized object type name: {})", subobject_type_id.Value(), subobject.GetType().name);

                continue;
            }

            if (component_interface->GetClass() != nullptr && component_interface->GetClass()->GetAttribute("serialize") == false) {
                HYP_LOG(Serialization, LogLevel::INFO, "HypClass for component '{}' has the Serialize attribute set to false; skipping", component_interface->GetTypeName());

                continue;
            }

            HYP_NAMED_SCOPE_FMT("Deserializing component '{}'", component_interface->GetTypeName());

            if (!subobject.m_deserialized_object) {
                return { FBOMResult::FBOM_ERR, HYP_FORMAT("No deserialized object found for component '{}'", component_interface->GetTypeName()) };
            }

            if (entity_manager->HasComponent(subobject_type_id, entity)) {
                return { FBOMResult::FBOM_ERR, HYP_FORMAT("Entity already has component '{}'", component_interface->GetTypeName()) };
            }

            entity_manager->AddComponent(entity, subobject.m_deserialized_object->ToRef());
        }

        out = HypData(entity);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Entity, FBOMMarshaler<Entity>);

// template <>
// class FBOMMarshaler<ID<Entity>> : public FBOMObjectMarshalerBase<ID<Entity>>
// {
// public:
//     virtual ~FBOMMarshaler() override = default;

//     virtual FBOMResult Serialize(const ID<Entity> &entity_id, FBOMObject &out) const override
//     {
//         return FBOMMarshaler<Handle<Entity>>{}.Serialize(Handle<Entity>(entity_id), out);
//     }

//     virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
//     {
//         return FBOMMarshaler<Handle<Entity>>{}.Deserialize(in, out);
//     }
// };

// HYP_DEFINE_MARSHAL(ID<Entity>, FBOMMarshaler<ID<Entity>>);

} // namespace hyperion::fbom