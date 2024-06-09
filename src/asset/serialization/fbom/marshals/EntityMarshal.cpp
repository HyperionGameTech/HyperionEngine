/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>

#include <scene/Entity.hpp>
#include <scene/ecs/EntityManager.hpp>

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

        out.SetProperty(NAME("num_components"), FBOMUnsignedInt(), uint32(all_components->Size()));

        uint32 component_index = 0;

        for (const auto &it : *all_components) {
            const ANSIString component_property_name = ANSIString("component_") + ANSIString::ToString(component_index);

            const void *component_ptr = entity_manager->TryGetComponent(it.first, entity.GetID());

            ComponentInterfaceBase *component_interface = entity_manager->GetComponentInterface(it.first);

            if (!component_interface) {
                return { FBOMResult::FBOM_ERR, "No ComponentInterface found for component" };
            }

            // write all properties
            FBOMObject component_object;
            component_object.SetProperty(NAME("type_id"), FBOMUnsignedInt(), component_interface->GetTypeID().Value());
            component_object.SetProperty(NAME("type_name"), FBOMString(component_interface->GetTypeName().Size()), component_interface->GetTypeName());
            component_object.SetProperty(NAME("num_properties"), FBOMUnsignedInt(), uint32(component_interface->GetProperties().Size()));

            // @TODO: Fix up when it is easy to create an array similar to how FBOMObject works. FBOMArray?
            uint32 property_index = 0;
            for (const ComponentProperty &property : component_interface->GetProperties()) {
                if (!property.IsReadable()) {
                    continue;
                }

                FBOMData property_value;
                property.GetGetter()(component_ptr, &property_value);

                const ANSIString property_name = ANSIString("property_") + ANSIString::ToString(property_index);

                FBOMObject property_object;
                property_object.SetProperty(NAME("name"), FBOMName(), property.GetName());
                property_object.SetProperty(NAME("value"), std::move(property_value));

                component_object.SetProperty(CreateNameFromDynamicString(property_name), FBOMData::FromObject(property_object));

                property_index++;
            }

            out.SetProperty(CreateNameFromDynamicString(component_property_name), FBOMData::FromObject(component_object));

            component_index++;
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        const Handle<Scene> &detached_scene = g_engine->GetWorld()->GetDetachedScene(ThreadID::Current());
        const RC<EntityManager> &entity_manager = detached_scene->GetEntityManager();

        const ID<Entity> entity = entity_manager->AddEntity();

        uint32 num_components;

        if (FBOMResult err = in.GetProperty("num_components").ReadUnsignedInt(&num_components)) {
            return err;
        }

        for (uint32 component_index = 0; component_index < num_components; component_index++) {
            FBOMObject component_object;

            if (FBOMResult err = in.GetProperty(CreateWeakNameFromDynamicString(ANSIString("component_") + ANSIString::ToString(component_index))).ReadObject(component_object)) {
                return err;
            }

            uint32 component_type_id_value;

            if (FBOMResult err = component_object.GetProperty("type_id").ReadUnsignedInt(&component_type_id_value)) {
                return err;
            }

            ComponentInterfaceBase *component_interface = entity_manager->GetComponentInterface(TypeID { component_type_id_value });

            if (!component_interface) {
                return { FBOMResult::FBOM_ERR, "No ComponentInterface for component" };
            }

            UniquePtr<void> component_ptr = component_interface->CreateComponent();
            AssertThrow(component_ptr != nullptr);

            uint32 num_properties;

            if (FBOMResult err = component_object.GetProperty("num_properties").ReadUnsignedInt(&num_properties)) {
                return err;
            }

            for (uint32 property_index = 0; property_index < num_properties; property_index++) {
                FBOMObject property_object;

                if (FBOMResult err = component_object.GetProperty(ANSIString("property_") + ANSIString::ToString(property_index)).ReadObject(property_object)) {
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

            entity_manager->AddComponent(entity, TypeID { component_type_id_value }, std::move(component_ptr));
        }

        out_object = Handle<Entity> { entity };

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Entity, FBOMMarshaler<Entity>);

} // namespace hyperion::fbom