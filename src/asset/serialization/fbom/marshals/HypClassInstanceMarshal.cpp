/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypMethod.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion::fbom {

FBOMResult HypClassInstanceMarshal::Serialize(ConstAnyRef in, FBOMObject &out) const
{
    if (!in.HasValue()) {
        return { FBOMResult::FBOM_ERR, "Attempting to serialize null object" };
    }

    const HypClass *hyp_class = GetClass(in.GetTypeID());

    if (!hyp_class) {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object using HypClassInstanceMarshal, TypeID {} has no associated HypClass", in.GetTypeID().Value()) };
    }

    HYP_LOG(Serialization, LogLevel::DEBUG, "Serializing object with HypClass '{}'...", hyp_class->GetName());

    out = FBOMObject(FBOMObjectType(hyp_class));

    for (HypProperty *property : hyp_class->GetProperties()) {
        AssertThrow(property != nullptr);

        if (!property->HasGetter()) {
            continue;
        }

        HYP_LOG(Serialization, LogLevel::DEBUG, "Serializing property '{}' on object with HypClass '{}'...", property->name, hyp_class->GetName());

        out.SetProperty(property->name.LookupString(), property->InvokeGetter_Serialized(in));
    }

    AnyRef non_const_any_ref { in.GetTypeID(), const_cast<void *>(in.GetPointer()) };
    HypData non_const_data { non_const_any_ref };

    for (HypMethod *method : hyp_class->GetMethods()) {
        AssertThrow(method != nullptr);

        if (method->params.Size() != 0) {
            // Cannot serialize methods with parameters. Needs to be a getter-type method.
            continue;
        }

        const String *serialize_as = method->GetAttribute("serializeas");

        if (!serialize_as) {
            continue;
        }

        HYP_LOG(Serialization, LogLevel::DEBUG, "Serializing result of method '{}' on object with HypClass '{}'...", method->name, hyp_class->GetName());

        out.SetProperty(serialize_as->Data(), method->Invoke_Serialized(Span<HypData> { &non_const_data, 1 }));
    }

    for (const HypField *field : hyp_class->GetFields()) {
        const String *serialize_as = field->GetAttribute("serializeas");

        if (!serialize_as) {
            continue;
        }

        HYP_LOG(Serialization, LogLevel::DEBUG, "Serializing field '{}' on object with HypClass '{}'...", field->name, hyp_class->GetName());

        out.SetProperty(serialize_as->Data(), field->Serialize(non_const_data));
    }

    HYP_LOG(Serialization, LogLevel::DEBUG, "Serialization completed for object with HypClass '{}'", hyp_class->GetName());

    return { FBOMResult::FBOM_OK };
}

FBOMResult HypClassInstanceMarshal::Deserialize(const FBOMObject &in, HypData &out) const
{
    const HypClass *hyp_class = in.GetHypClass();

    if (!hyp_class) {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object using HypClassInstanceMarshal, serialized type '{}' has no associated HypClass", in.GetType().name) };
    }

    hyp_class->CreateInstance(out);

    AnyRef ref = out.ToRef();
    AssertThrowMsg(ref.HasValue(), "Failed to create HypClass instance");

    return Deserialize_Internal(in, hyp_class, ref);
}

FBOMResult HypClassInstanceMarshal::Deserialize_Internal(const FBOMObject &in, const HypClass *hyp_class, AnyRef ref) const
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(ref.HasValue());

    HypData target_data { ref };

    HashMap<WeakName, FlatSet<const HypMethod *>> deserialize_methods;
    HashMap<WeakName, FlatSet<const HypField *>> deserialize_fields;

    for (const HypMethod *method : hyp_class->GetMethods()) {
        const String *serialize_as = method->GetAttribute("serializeas");

        if (!serialize_as) {
            continue;
        }

        if (method->params.Size() != 1) {
            // Cannot deserialize to methods with > 1 parameter; must be a setter-type method.
            continue;
        }

        auto deserialize_methods_it = deserialize_methods.Find(serialize_as->Data());

        if (deserialize_methods_it == deserialize_methods.End()) {
            deserialize_methods_it = deserialize_methods.Insert(serialize_as->Data(), { }).first;
        }

        deserialize_methods_it->second.Insert(method);
    }

    for (const HypField *field : hyp_class->GetFields()) {
        const String *serialize_as = field->GetAttribute("serializeas");

        if (!serialize_as) {
            continue;
        }

        auto deserialize_fields_it = deserialize_fields.FindAs(serialize_as->Data());

        if (deserialize_fields_it == deserialize_fields.End()) {
            deserialize_fields_it = deserialize_fields.Insert(serialize_as->Data(), { }).first;
        }

        deserialize_fields_it->second.Insert(field);
    }
    
    for (const KeyValuePair<ANSIString, FBOMData> &it : in.GetProperties()) {
        if (const HypProperty *property = hyp_class->GetProperty(it.first)) {
            if (!property->HasSetter()) {
                HYP_LOG(Serialization, LogLevel::WARNING, "Property {} on HypClass {} has no setter", it.first, hyp_class->GetName());

                continue;
            }

            property->InvokeSetter_Serialized(ref, it.second);
        }

        auto deserialize_methods_it = deserialize_methods.FindAs(it.first);

        if (deserialize_methods_it != deserialize_methods.End()) {
            for (const HypMethod *method : deserialize_methods_it->second) {
                HYP_LOG(Serialization, LogLevel::DEBUG, "Deserializing property '{}' on object, calling setter {} for HypClass '{}'...", it.first, method->name, hyp_class->GetName());

                method->Invoke_Deserialized(Span<HypData> { &target_data, 1 }, it.second);
            }
        }

        auto deserialize_fields_it = deserialize_fields.FindAs(it.first);

        if (deserialize_fields_it != deserialize_fields.End()) {
            for (const HypField *field : deserialize_fields_it->second) {
                HYP_LOG(Serialization, LogLevel::DEBUG, "Deserializing property '{}' on object, setting field {} for HypClass '{}'...", it.first, field->name, hyp_class->GetName());

                field->Deserialize(target_data, it.second);
            }
        }
    }

    return { FBOMResult::FBOM_OK };
}

} // namespace hyperion::fbom