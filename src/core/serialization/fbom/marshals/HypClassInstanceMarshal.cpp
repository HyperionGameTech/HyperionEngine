/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypStruct.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypObjectBase.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion::serialization {

FBOMResult HypClassInstanceMarshal::Serialize(ConstAnyRef in, FBOMObject& out) const
{
    if (!in.HasValue())
    {
        return { FBOMResult::FBOM_ERR, "Attempting to serialize null object" };
    }

    const HypClass* hypClass = GetClass(in.GetTypeId());

    if (!hypClass)
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object using HypClassInstanceMarshal, TypeId {} has no associated HypClass", LookupTypeName(in.GetTypeId())) };
    }

    if (hypClass->IsClassType())
    {
        // Get instance class if object is a HypObject
        const HypClass* instanceClass = static_cast<const HypObjectBase*>(in.GetPointer())->InstanceClass();
        Assert(instanceClass != nullptr);

        hypClass = instanceClass;
    }

    if (!hypClass->CanSerialize())
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object using HypClassInstanceMarshal, TypeId {} has no associated HypClass", LookupTypeName(in.GetTypeId())) };
    }

    const HypClassAttributeValue& serializeAttribute = hypClass->GetAttribute("serialize");

    if (serializeAttribute == false)
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}', HypClass has attribute \"serialize\"=false", hypClass->GetName()) };
    }

    HYP_NAMED_SCOPE_FMT("Serializing object with HypClass '{}'", hypClass->GetName());

    if (hypClass->GetSerializationMode() & HypClassSerializationMode::BITWISE)
    {
        if (!hypClass->IsStructType())
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}', HypClass has attribute \"serialize\"=\"bitwise\" but is not a struct type", hypClass->GetName()) };
        }

        const HypStruct* hypStruct = static_cast<const HypStruct*>(hypClass);

        if (FBOMResult err = hypStruct->SerializeStruct(in, out))
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}': {}", hypClass->GetName(), err.message) };
        }

        return { FBOMResult::FBOM_OK };
    }

    HypData targetData { AnyRef { hypClass->GetTypeId(), const_cast<void*>(in.GetPointer()) } };

    out = FBOMObject(FBOMObjectType(hypClass));

    {
        HYP_NAMED_SCOPE_FMT("Serializing properties for HypClass '{}'", hypClass->GetName());

        for (IHypMember& member : hypClass->GetMembers(HypMemberType::TYPE_PROPERTY))
        {
            if (!member.CanSerialize())
            {
                continue;
            }

            if (!member.GetAttribute("serialize"))
            {
                continue;
            }

            HYP_NAMED_SCOPE_FMT("Serializing member '{}' for HypClass '{}'", member.GetName(), hypClass->GetName());

            EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE;

            if (member.GetAttribute("compressed").GetBool())
            {
                flags |= FBOMDataFlags::COMPRESSED;
            }

            FBOMData data;

            if (!member.Serialize(Span<HypData>(&targetData, 1), data, flags))
            {
                return { FBOMResult::FBOM_ERR, HYP_FORMAT("Failed to serialize member '{}' of HypClass '{}'", member.GetName(), hypClass->GetName()) };
            }

            out.SetProperty(member.GetName().LookupString(), std::move(data));
        }
    }

    return { FBOMResult::FBOM_OK };
}

FBOMResult HypClassInstanceMarshal::Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const
{
    const HypClass* hypClass = in.GetHypClass();

    if (!hypClass)
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object using HypClassInstanceMarshal, serialized data with type '{}' (TypeId: {}) has no associated HypClass", in.GetType().name, in.GetType().GetNativeTypeId().Value()) };
    }

    if (!hypClass->CreateInstance(out))
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object using HypClassInstanceMarshal, HypClass '{}' instance creation failed", hypClass->GetName()) };
    }

    if (hypClass->GetSerializationMode() & HypClassSerializationMode::BITWISE)
    {
        if (!hypClass->IsStructType())
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}', HypClass has attribute \"serialize\"=\"bitwise\" but is not a struct type", hypClass->GetName()) };
        }

        const HypStruct* hypStruct = static_cast<const HypStruct*>(hypClass);

        return hypStruct->DeserializeStruct(context, in, out);
    }

    AnyRef ref = out.ToRef();
    HYP_CORE_ASSERT(ref.HasValue(), "Failed to create HypClass instance");

    if (FBOMResult err = Deserialize_Internal(context, in, hypClass, ref))
    {
        return err;
    }

    return { FBOMResult::FBOM_OK };
}

FBOMResult HypClassInstanceMarshal::Deserialize_Internal(FBOMLoadContext& context, const FBOMObject& in, const HypClass* hypClass, AnyRef ref) const
{
    HYP_CORE_ASSERT(hypClass != nullptr);
    HYP_CORE_ASSERT(ref.HasValue());

    HypData targetData { ref };

    {
        HYP_NAMED_SCOPE_FMT("Deserializing properties for HypClass '{}'", hypClass->GetName());

        for (const KeyValuePair<ANSIString, FBOMData>& it : in.GetProperties())
        {
            if (const HypProperty* property = hypClass->GetProperty(it.first))
            {
                if (!property->GetAttribute("serialize"))
                {
                    continue;
                }

                if (!property->CanDeserialize())
                {
                    HYP_NAMED_SCOPE_FMT("Deserializing member '{}' on object, skipping setter for HypClass '{}'", property->GetName(), hypClass->GetName());

                    continue;
                }

                if (!property->Deserialize(context, targetData, it.second))
                {
                    return { FBOMResult::FBOM_ERR, HYP_FORMAT("Failed to deserialize member '{}' of HypClass '{}'", property->GetName(), hypClass->GetName()) };
                }
            }
        }
    }

    hypClass->PostLoad(ref.GetPointer());

    return { FBOMResult::FBOM_OK };
}

} // namespace hyperion::serialization