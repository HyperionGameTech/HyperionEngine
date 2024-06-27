/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>

#include <scene/Entity.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

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
            const void *component_ptr = entity_manager->TryGetComponent(it.first, entity.GetID());

            ComponentInterfaceBase *component_interface = entity_manager->GetComponentInterface(it.first);

            if (!component_interface) {
                continue; // skip, for now.
                // return { FBOMResult::FBOM_ERR, "No ComponentInterface found for component" };
            }

            // write all properties
            FBOMObject component_object;
            component_object.SetProperty(NAME("type_id"), FBOMData::FromUnsignedInt(component_interface->GetTypeID().Value()));
            component_object.SetProperty(NAME("type_name"), component_interface->GetTypeName());

            FBOMArray properties_array;

            for (const ComponentProperty &property : component_interface->GetProperties()) {
                if (!property.IsReadable()) {
                    continue;
                }

                FBOMData property_value;
                property.GetGetter()(component_ptr, &property_value);

                FBOMObject property_object;
                property_object.SetProperty(NAME("name"), FBOMData::FromName(property.GetName()));
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

            if (entity_manager->HasComponent(component_type_id, entity)) {
                HYP_LOG(Serialization, LogLevel::WARNING, "Entity already has component of type {}", component_type_id.Value());

                continue;
            }

            ComponentInterfaceBase *component_interface = entity_manager->GetComponentInterface(component_type_id);

            if (!component_interface) {
                return { FBOMResult::FBOM_ERR, "No ComponentInterface for component" };
            }

            UniquePtr<void> component_ptr = component_interface->CreateComponent();
            AssertThrow(component_ptr != nullptr);

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

                ComponentProperty *component_property = component_interface->GetProperty(property_name);

                if (!component_property) {
                    return { FBOMResult::FBOM_ERR, "Invalid property referenced" };
                }
                
                if (!component_property->IsWritable()) {
                    continue;
                }

                const FBOMData &property_value = property_object.GetProperty("value");

                component_property->GetSetter()(component_ptr.Get(), &property_value);
            }

            entity_manager->AddComponent(entity, component_type_id, std::move(component_ptr));
        }

        out_object = Handle<Entity> { entity };

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Entity, FBOMMarshaler<Entity>);

} // namespace hyperion::fbom