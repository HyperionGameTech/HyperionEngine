/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypClassRegistry.hpp>

namespace hyperion {

const HypClass *HypProperty::GetHypClass() const
{
    return HypClassRegistry::GetInstance().GetClass(type_id);
}


HypProperty HypProperty::MakeHypProperty(const HypField *field)
{
    AssertThrow(field != nullptr);

    HypProperty result;
    result.name = field->name;
    result.type_id = field->type_id;

    result.getter = HypPropertyGetter();
    result.getter.type_info.target_type_id = field->target_type_id;
    result.getter.type_info.value_type_id = field->type_id;
    result.getter.get_proc = [field](const HypData &target) -> HypData
    {
        return field->Get(target);
    };
    result.getter.serialize_proc = [field](const HypData &target) -> fbom::FBOMData
    {
        return field->Serialize(target);
    };

    result.setter = HypPropertySetter();
    result.setter.type_info.target_type_id = field->target_type_id;
    result.setter.type_info.value_type_id = field->type_id;
    result.setter.set_proc = [field](HypData &target, const HypData &value) -> void
    {
        field->Set(target, value);
    };
    result.setter.deserialize_proc = [field](HypData &target, const fbom::FBOMData &value) -> void
    {
        field->Deserialize(target, value);
    };

    return result;
}

HypProperty HypProperty::MakeHypProperty(const HypMethod *getter, const HypMethod *setter)
{
    HypProperty result;

    const String *property_attribute = nullptr;

    Optional<TypeID> type_id;
    Optional<TypeID> target_type_id;
    
    const bool has_getter = getter != nullptr && getter->params.Size() >= 1;
    const bool has_setter = setter != nullptr && setter->params.Size() >= 2;

    if (has_getter) {
        property_attribute = getter->GetAttribute("property");

        type_id = getter->return_type_id;
        target_type_id = getter->params[0].type_id;
    }

    if (has_setter) {
        if (!property_attribute) {
            property_attribute = setter->GetAttribute("property");
        }

        const TypeID setter_type_id = setter->params[0].type_id;

        if (type_id.HasValue()) {
            AssertThrowMsg(*type_id == setter_type_id, "Getter TypeID (%u) does not match setter TypeID (%u)", type_id->Value(), setter_type_id.Value());
        } else {
            type_id = setter_type_id;
        }

        if (!type_id.HasValue()) {
            type_id = setter_type_id;
        }

        if (target_type_id.HasValue()) {
            AssertThrowMsg(*target_type_id == setter->target_type_id, "Getter target TypeID (%u) does not match setter target TypeID (%u)", target_type_id->Value(), setter->target_type_id.Value());
        } else {
            target_type_id = setter->target_type_id;
        }
    }

    AssertThrowMsg(property_attribute != nullptr, "A HypProperty composed of getter/setter pair must have at least one method that has \"Property=\" attribute");
    AssertThrowMsg(type_id.HasValue(), "Cannot determine TypeID from getter/setter pair");

    result.name = CreateNameFromDynamicString(*property_attribute);
    result.type_id = *type_id;

    if (has_getter) {
        result.getter = HypPropertyGetter();
        result.getter.type_info.target_type_id = *target_type_id;
        result.getter.type_info.value_type_id = *type_id;
        result.getter.get_proc = [getter](const HypData &target) -> HypData
        {
            return getter->Invoke(Span<HypData> { const_cast<HypData *>(&target), 1 });
        };
        result.getter.serialize_proc = [getter](const HypData &target) -> fbom::FBOMData
        {
            return getter->Invoke_Serialized(Span<HypData> { const_cast<HypData *>(&target), 1 });
        };
    }

    if (has_setter) {
        result.setter = HypPropertySetter();
        result.setter.type_info.target_type_id = *target_type_id;
        result.setter.type_info.value_type_id = setter->params[0].type_id;
        result.setter.set_proc = [setter](HypData &target, const HypData &value) -> void
        {
            HYP_NOT_IMPLEMENTED_VOID();
            // setter->Invoke(Span<HypData> { const_cast<HypData *>(&target), 1 });
        };
        result.setter.deserialize_proc = [setter](HypData &target, const fbom::FBOMData &value) -> void
        {
            setter->Invoke_Deserialized(Span<HypData> { const_cast<HypData *>(&target), 1 }, value);
        };
    }

    return result;
}

} // namespace hyperion