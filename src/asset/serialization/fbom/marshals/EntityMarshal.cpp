/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>

#include <scene/Entity.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>

#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Entity> : public FBOMObjectMarshalerBase<Entity>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const Entity &entity, FBOMObject &out) const override
    {
        EntityManager *entity_manager = EntityManager::GetEntityToEntityManagerMap().GetEntityManager(entity.GetID());

        if (!entity_manager) {
            return { FBOMResult::FBOM_ERR, "Entity not attached to an EntityManager" };
        }

        Optional<const TypeMap<ComponentID> &> all_components = entity_manager->GetAllComponents(entity.GetID());

        if (!all_components.HasValue()) {
            return { FBOMResult::FBOM_ERR, "No component map found for entity" };
        }

        FBOMArray components_array;

        for (const auto &it : *all_components) {
            const TypeID component_type_id = it.first;
            const HypClass *component_class = GetClass(component_type_id);

            if (!component_class) {
                continue; // skip, for now.
                // return { FBOMResult::FBOM_ERR, "No HypClass found for component" };
            }

            ConstAnyRef component_ptr = entity_manager->TryGetComponent(component_type_id, entity.GetID());

            // write all properties
            FBOMObject component_object;
            component_object.SetProperty(NAME("type_id"), FBOMData::FromUnsignedInt(component_type_id.Value()));
            component_object.SetProperty(NAME("type_name"), FBOMData::FromName(component_class->GetName()));

            FBOMArray properties_array;

            for (const HypProperty *property : component_class->GetProperties()) {
                if (!property->HasGetter()) {
                    continue;
                }

                FBOMData property_value = property->InvokeGetter(component_ptr);

                FBOMObject property_object;
                property_object.SetProperty(NAME("name"), FBOMData::FromName(property->name));
                property_object.SetProperty(NAME("value"), std::move(property_value));

                properties_array.AddElement(FBOMData::FromObject(property_object));
            }

            component_object.SetProperty(NAME("properties"), FBOMData::FromArray(std::move(properties_array)));

            components_array.AddElement(FBOMData::FromObject(component_object));
        }

        out.SetProperty(NAME("components"), FBOMData::FromArray(std::move(components_array)));

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        const Handle<Scene> &detached_scene = g_engine->GetWorld()->GetDetachedScene(ThreadID::Current());
        const RC<EntityManager> &entity_manager = detached_scene->GetEntityManager();

        const ID<Entity> entity = entity_manager->AddEntity();

        FBOMArray components_array;

        if (FBOMResult err = in.GetProperty("components").ReadArray(components_array)) {
            return err;
        }

        for (SizeType component_index = 0; component_index < components_array.Size(); component_index++) {
            FBOMObject component_object;

            if (FBOMResult err = components_array.GetElement(component_index).ReadObject(component_object)) {
                return err;
            }

            uint32 component_type_id_value;

            if (FBOMResult err = component_object.GetProperty("type_id").ReadUnsignedInt(&component_type_id_value)) {
                return err;
            }

            const TypeID component_type_id { component_type_id_value };

            const HypClass *component_class = GetClass(component_type_id);

            if (!component_class) {
                HYP_LOG(Serialization, LogLevel::WARNING, "No HypClass registered for component with type {}", component_type_id.Value());

                continue;
            }

            if (entity_manager->HasComponent(component_type_id, entity)) {
                HYP_LOG(Serialization, LogLevel::WARNING, "Entity already has component of type {}", component_type_id.Value());

                continue;
            }

            Any component_instance;
            component_class->CreateInstance(component_instance);
            AssertThrow(component_instance.HasValue());

            FBOMArray properties_array;

            if (FBOMResult err = component_object.GetProperty("properties").ReadArray(properties_array)) {
                return err;
            }

            for (SizeType property_index = 0; property_index < properties_array.Size(); property_index++) {
                FBOMObject property_object;

                if (FBOMResult err = properties_array.GetElement(property_index).ReadObject(property_object)) {
                    return err;
                }

                Name property_name;

                if (FBOMResult err = property_object.GetProperty("name").ReadName(&property_name)) {
                    return err;
                }

                HypProperty *component_property = component_class->GetProperty(property_name);

                if (!component_property) {
                    return { FBOMResult::FBOM_ERR, "Invalid property referenced" };
                }
                
                if (!component_property->HasSetter()) {
                    continue;
                }

                const FBOMData &property_value = property_object.GetProperty("value");

                component_property->InvokeSetter(component_instance, property_value);
            }

            entity_manager->AddComponent(entity, component_type_id, UniquePtr<void>(std::move(component_instance)));
        }

        out_object = Handle<Entity> { entity };

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Entity, FBOMMarshaler<Entity>);

} // namespace hyperion::fbom