/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypClassRegistry.hpp>

namespace hyperion {

const HypClass *HypProperty::GetHypClass() const
{
    return HypClassRegistry::GetInstance().GetClass(m_type_id);
}

HypProperty HypProperty::MakeHypProperty(const HypField *field)
{
    AssertThrow(field != nullptr);

    Name property_name;

    if (const HypClassAttributeValue &attr = field->GetAttribute("property"); attr.IsString()) {
        property_name = CreateNameFromDynamicString(attr.GetString());
    } else {
        property_name = field->GetName();
    }

    HypProperty result;
    result.m_name = property_name;
    result.m_type_id = field->GetTypeID();
    result.m_attributes = field->GetAttributes();

    result.m_getter = HypPropertyGetter();
    result.m_getter.type_info.target_type_id = field->GetTargetTypeID();
    result.m_getter.type_info.value_type_id = field->GetTypeID();
    result.m_getter.get_proc = [field](const HypData &target) -> HypData
    {
        return field->Get(target);
    };
    result.m_getter.serialize_proc = [field](const HypData &target) -> fbom::FBOMData
    {
        fbom::FBOMData data;

        if (!field->Serialize(target, data)) {
            return fbom::FBOMData();
        }

        return data;
    };

    result.m_setter = HypPropertySetter();
    result.m_setter.type_info.target_type_id = field->GetTargetTypeID();
    result.m_setter.type_info.value_type_id = field->GetTypeID();
    result.m_setter.set_proc = [field](HypData &target, const HypData &value) -> void
    {
        field->Set(target, value);
    };
    result.m_setter.deserialize_proc = [field](HypData &target, const fbom::FBOMData &value) -> void
    {
        field->Deserialize(target, value);
    };

    return result;
}

HypProperty HypProperty::MakeHypProperty(const HypMethod *getter, const HypMethod *setter)
{
    HypProperty result;

    Optional<String> property_attribute_opt;

    Optional<TypeID> type_id;
    Optional<TypeID> target_type_id;
    
    const bool has_getter = getter != nullptr && getter->GetParameters().Size() >= 1;
    const bool has_setter = setter != nullptr && setter->GetParameters().Size() >= 2;

    if (has_getter) {
        if (const HypClassAttributeValue &attr = getter->GetAttribute("property")) {
            property_attribute_opt = attr.GetString();
        }

        type_id = getter->GetTypeID();
        target_type_id = getter->GetParameters()[0].type_id;

        result.m_attributes = getter->GetAttributes();
    }

    if (has_setter) {
        if (!property_attribute_opt) {
            if (const HypClassAttributeValue &attr = setter->GetAttribute("property")) {
                property_attribute_opt = attr.GetString();
            }
        }

        const TypeID setter_type_id = setter->GetParameters()[0].type_id;

        if (type_id.HasValue()) {
            AssertThrowMsg(*type_id == setter_type_id, "Getter TypeID (%u) does not match setter TypeID (%u)", type_id->Value(), setter_type_id.Value());
        } else {
            type_id = setter_type_id;
        }

        if (!type_id.HasValue()) {
            type_id = setter_type_id;
        }

        if (target_type_id.HasValue()) {
            AssertThrowMsg(*target_type_id == setter->GetTargetTypeID(), "Getter target TypeID (%u) does not match setter target TypeID (%u)", target_type_id->Value(), setter->GetTargetTypeID().Value());
        } else {
            target_type_id = setter->GetTargetTypeID();
        }

        result.m_attributes.Merge(setter->GetAttributes());
    }

    AssertThrowMsg(property_attribute_opt.HasValue(), "A HypProperty composed of getter/setter pair must have at least one method that has \"Property=\" attribute");
    AssertThrowMsg(type_id.HasValue(), "Cannot determine TypeID from getter/setter pair");

    result.m_name = CreateNameFromDynamicString(*property_attribute_opt);
    result.m_type_id = *type_id;

    if (has_getter) {
        result.m_getter = HypPropertyGetter();
        result.m_getter.type_info.target_type_id = *target_type_id;
        result.m_getter.type_info.value_type_id = *type_id;
        result.m_getter.get_proc = [getter](const HypData &target) -> HypData
        {
            return getter->Invoke(Span<HypData> { const_cast<HypData *>(&target), 1 });
        };
        result.m_getter.serialize_proc = [getter](const HypData &target) -> fbom::FBOMData
        {
            fbom::FBOMData data;
            AssertThrow(getter->Serialize(Span<HypData> { const_cast<HypData *>(&target), 1 }, data));

            return data;
        };
    }

    if (has_setter) {
        result.m_setter = HypPropertySetter();
        result.m_setter.type_info.target_type_id = *target_type_id;
        result.m_setter.type_info.value_type_id = setter->GetParameters()[0].type_id;
        result.m_setter.set_proc = [setter](HypData &target, const HypData &value) -> void
        {
            setter->Invoke(Array<HypData *> { &target, const_cast<HypData *>(&value) });
        };
        result.m_setter.deserialize_proc = [setter](HypData &target, const fbom::FBOMData &value) -> void
        {
            AssertThrow(setter->Deserialize(target, value));
        };
    }

    return result;
}

} // namespace hyperion