/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypStruct.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypMethod.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

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

    if (!hyp_class->CanSerialize()) {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object using HypClassInstanceMarshal, TypeID {} has no associated HypClass", in.GetTypeID().Value()) };
    }

    const HypClassAttributeValue &serialize_attribute = hyp_class->GetAttribute("serialize");

    if (serialize_attribute == false) {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}', HypClass has attribute \"serialize\"=false", hyp_class->GetName()) };
    }

    HYP_NAMED_SCOPE_FMT("Serializing object with HypClass '{}'", hyp_class->GetName());

    if (hyp_class->GetSerializationMode() & HypClassSerializationMode::BITWISE) {
        if (!hyp_class->IsStructType()) {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}', HypClass has attribute \"serialize\"=\"bitwise\" but is not a struct type", hyp_class->GetName()) };
        }

        const HypStruct *hyp_struct = static_cast<const HypStruct *>(hyp_class);

        if (FBOMResult err = hyp_struct->SerializeStruct(in, out)) {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}': {}", hyp_class->GetName(), err.message) };
        }

        return { FBOMResult::FBOM_OK };
    }

    HypData target_data { AnyRef { in.GetTypeID(), const_cast<void *>(in.GetPointer()) } };

    out = FBOMObject(FBOMObjectType(hyp_class));

    {
        HYP_NAMED_SCOPE_FMT("Serializing properties for HypClass '{}'", hyp_class->GetName());

        for (IHypMember &member : hyp_class->GetMembers(HypMemberType::TYPE_PROPERTY)) {
            if (!member.CanSerialize()) {
                continue;
            }

            if (!member.GetAttribute("serialize")) {
                continue;
            }

            HYP_NAMED_SCOPE_FMT("Serializing member '{}' for HypClass '{}'", member.GetName(), hyp_class->GetName());

            fbom::FBOMData data;

            if (!member.Serialize(Span<HypData>(&target_data, 1), data)) {
                return { FBOMResult::FBOM_ERR, HYP_FORMAT("Failed to serialize member '{}' of HypClass '{}'", member.GetName(), hyp_class->GetName()) };
            }

            out.SetProperty(member.GetName().LookupString(), std::move(data));
        }
    }

    return { FBOMResult::FBOM_OK };
}

FBOMResult HypClassInstanceMarshal::Deserialize(FBOMLoadContext &context, const FBOMObject &in, HypData &out) const
{
    const HypClass *hyp_class = in.GetHypClass();

    if (!hyp_class) {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object using HypClassInstanceMarshal, serialized data with type '{}' (TypeID: {}) has no associated HypClass", in.GetType().name, in.GetType().GetNativeTypeID().Value()) };
    }

    hyp_class->CreateInstance(out);

    if (hyp_class->GetSerializationMode() & HypClassSerializationMode::BITWISE) {
        if (!hyp_class->IsStructType()) {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}', HypClass has attribute \"serialize\"=\"bitwise\" but is not a struct type", hyp_class->GetName()) };
        }

        const HypStruct *hyp_struct = static_cast<const HypStruct *>(hyp_class);

        return hyp_struct->DeserializeStruct(context, in, out);
    }

    AnyRef ref = out.ToRef();
    AssertThrowMsg(ref.HasValue(), "Failed to create HypClass instance");

    if (FBOMResult err = Deserialize_Internal(context, in, hyp_class, ref)) {
        return err;
    }

    return { FBOMResult::FBOM_OK };
}

FBOMResult HypClassInstanceMarshal::Deserialize_Internal(FBOMLoadContext &context, const FBOMObject &in, const HypClass *hyp_class, AnyRef ref) const
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(ref.HasValue());

    HypData target_data { ref };

    {
        HYP_NAMED_SCOPE_FMT("Deserializing properties for HypClass '{}'", hyp_class->GetName());

        for (const KeyValuePair<ANSIString, FBOMData> &it : in.GetProperties()) {
            if (const HypProperty *property = hyp_class->GetProperty(it.first)) {
                if (!property->GetAttribute("serialize")) {
                    continue;
                }

                if (!property->CanDeserialize()) {
                    HYP_NAMED_SCOPE_FMT("Deserializing member '{}' on object, skipping setter for HypClass '{}'", property->GetName(), hyp_class->GetName());

                    continue;
                }

                if (!property->Deserialize(context, target_data, it.second)) {
                    return { FBOMResult::FBOM_ERR, HYP_FORMAT("Failed to deserialize member '{}' of HypClass '{}'", property->GetName(), hyp_class->GetName()) };
                }
            }
        }
    }

    hyp_class->PostLoad(ref.GetPointer());

    return { FBOMResult::FBOM_OK };
}

} // namespace hyperion::fbom